// USB host transport: the ESP32 acts as USB host and plugs straight into
// the RP2040's own USB-C port, talking to it as the CDC-ACM serial device
// it already is - no extra wiring beyond the one USB-C to USB-C cable. See
// docs/esp32-network-bridge.md's "USB host link" section.
//
// Needs an ESP32-S2/S3/P4 (native USB-OTG) built via the `esp32s3_usbhost`
// PlatformIO environment (framework = arduino, espidf, so the plain-C
// usb_host_cdc_acm component below is available alongside Arduino's
// Stream/String).
//
// The Espressif docs are explicit that the USB Host Library creates no
// tasks of its own - the application has to run its own "daemon" task
// pumping usb_host_lib_handle_events() for as long as the host stack is
// installed. That task and the one (re)opening the CDC device are kept
// separate below so the second can block waiting for a device to
// enumerate without starving the first - the enumeration it's waiting on
// only happens via events the daemon task pumps.
//
// Self-excluded by JACK_LINK_USB_HOST (set in platformio.ini's
// `esp32s3_usbhost` env only) - see uart_link.cpp's file comment for why
// this can't just rely on build_src_filter instead.
#ifdef JACK_LINK_USB_HOST

#include "jack_link.h"

#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <freertos/task.h>

#include "esp_intr_alloc.h"
#include "usb/cdc_acm_host.h"
#include "usb/usb_host.h"

namespace {
// CDC_HOST_ANY_VID/PID accept any standards-compliant CDC-ACM device,
// rather than hardcoding arduino-pico's confirmed VID:PID (2E8A:000A) -
// keeps this working if the RP2040 side's USB stack/core ever changes.
constexpr uint16_t kJackVid = CDC_HOST_ANY_VID;
constexpr uint16_t kJackPid = CDC_HOST_ANY_PID;
constexpr uint8_t kJackInterfaceIdx = 0;

constexpr size_t kUsbBufferSize = 256;
constexpr uint32_t kOpenTimeoutMs = 2000;
constexpr uint32_t kOpenRetryDelayMs = 500;

constexpr size_t kRingBufferCapacity = 2048;

// Single-producer (the driver's own task, via handleRxData)/single-consumer
// (the Arduino main loop, via UsbHostCdcStream) byte ring buffer. A mutex
// is simplest here since neither side is an ISR - the data callback runs
// in the CDC driver's own FreeRTOS task context, not an interrupt handler.
class RingBuffer {
public:
  RingBuffer() { mutex_ = xSemaphoreCreateMutex(); }

  void write(const uint8_t *data, size_t len) {
    if (xSemaphoreTake(mutex_, portMAX_DELAY) != pdTRUE) {
      return;
    }
    for (size_t i = 0; i < len; i++) {
      size_t next = (head_ + 1) % kRingBufferCapacity;
      if (next == tail_) {
        // Full - drop the oldest byte rather than blocking the USB
        // driver's task, which would stall other USB traffic.
        tail_ = (tail_ + 1) % kRingBufferCapacity;
      }
      buf_[head_] = data[i];
      head_ = next;
    }
    xSemaphoreGive(mutex_);
  }

  int read() {
    if (xSemaphoreTake(mutex_, portMAX_DELAY) != pdTRUE) {
      return -1;
    }
    int result = -1;
    if (tail_ != head_) {
      result = buf_[tail_];
      tail_ = (tail_ + 1) % kRingBufferCapacity;
    }
    xSemaphoreGive(mutex_);
    return result;
  }

  int available() {
    if (xSemaphoreTake(mutex_, portMAX_DELAY) != pdTRUE) {
      return 0;
    }
    int n = (int)((head_ + kRingBufferCapacity - tail_) % kRingBufferCapacity);
    xSemaphoreGive(mutex_);
    return n;
  }

private:
  SemaphoreHandle_t mutex_;
  uint8_t buf_[kRingBufferCapacity];
  size_t head_ = 0;
  size_t tail_ = 0;
};

RingBuffer g_rxBuffer;
cdc_acm_dev_hdl_t g_cdcDev = nullptr;
volatile bool g_deviceReady = false;

bool handleRxData(const uint8_t *data, size_t data_len, void *user_arg) {
  g_rxBuffer.write(data, data_len);
  return true; // fully consumed into the ring buffer
}

void handleDevEvent(const cdc_acm_host_dev_event_data_t *event, void *user_ctx) {
  if (event->type == CDC_ACM_HOST_DEVICE_DISCONNECTED) {
    g_deviceReady = false;
    cdc_acm_host_close(event->data.cdc_hdl);
    g_cdcDev = nullptr;
  }
}

// The mandatory "daemon" task - see the file comment above.
void usbDaemonTask(void *arg) {
  for (;;) {
    uint32_t eventFlags = 0;
    usb_host_lib_handle_events(portMAX_DELAY, &eventFlags);
  }
}

// (Re)opens the RP2040's CDC-ACM interface whenever it isn't currently
// open, so unplugging/replugging the USB cable recovers on its own.
void usbCdcOpenTask(void *arg) {
  for (;;) {
    if (!g_deviceReady) {
      cdc_acm_host_device_config_t devConfig = {};
      devConfig.connection_timeout_ms = kOpenTimeoutMs;
      devConfig.out_buffer_size = kUsbBufferSize;
      devConfig.in_buffer_size = kUsbBufferSize;
      devConfig.event_cb = handleDevEvent;
      devConfig.data_cb = handleRxData;
      devConfig.user_arg = nullptr;

      esp_err_t err = cdc_acm_host_open(kJackVid, kJackPid, kJackInterfaceIdx,
                                        &devConfig, &g_cdcDev);
      if (err == ESP_OK) {
        g_deviceReady = true;
      }
    }
    vTaskDelay(pdMS_TO_TICKS(kOpenRetryDelayMs));
  }
}

class UsbHostCdcStream : public Stream {
public:
  int available() override { return g_rxBuffer.available(); }
  int read() override { return g_rxBuffer.read(); }
  int peek() override { return -1; } // unused by sendCommandAndRead()
  size_t write(uint8_t b) override { return write(&b, 1); }
  size_t write(const uint8_t *buffer, size_t size) override {
    if (!g_deviceReady) {
      return 0;
    }
    esp_err_t err = cdc_acm_host_data_tx_blocking(g_cdcDev, buffer, size, 1000);
    return err == ESP_OK ? size : 0;
  }
};

UsbHostCdcStream g_stream;
} // namespace

void jackLinkBegin() {
  usb_host_config_t hostConfig = {};
  hostConfig.intr_flags = ESP_INTR_FLAG_LEVEL1;
  ESP_ERROR_CHECK(usb_host_install(&hostConfig));

  cdc_acm_host_driver_config_t driverConfig = {};
  driverConfig.driver_task_stack_size = 4096;
  driverConfig.driver_task_priority = 5;
  driverConfig.xCoreID = 0;
  driverConfig.new_dev_cb = nullptr; // not used - see usbCdcOpenTask instead
  ESP_ERROR_CHECK(cdc_acm_host_install(&driverConfig));

  xTaskCreatePinnedToCore(usbDaemonTask, "usb_daemon", 4096, nullptr, 6,
                          nullptr, 0);
  xTaskCreatePinnedToCore(usbCdcOpenTask, "usb_cdc_open", 4096, nullptr, 5,
                          nullptr, 0);
}

Stream &jackLink() { return g_stream; }

bool jackLinkReady() { return g_deviceReady; }

// No GPIO pins to configure for a USB pipe - see jack_link.h.
bool jackLinkSupportsPinConfig() { return false; }

void jackLinkGetPins(int *rxPin, int *txPin) {
  *rxPin = -1;
  *txPin = -1;
}

bool jackLinkSetPins(int rxPin, int txPin) { return false; }

#endif // JACK_LINK_USB_HOST
