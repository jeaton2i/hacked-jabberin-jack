# Vendored component

Copied from [espressif/esp-usb](https://github.com/espressif/esp-usb),
path `host/class/cdc/usb_host_cdc_acm` (component manifest version 2.4.1 at
copy time), unmodified except for removing `idf_component.yml`, `test_app/`,
and `host_test/` - see below. Licensed under Apache 2.0 (see `LICENSE` in
this directory).

## Why vendored instead of a managed `idf_component.yml` dependency

PlatformIO's `framework = arduino, espidf` hybrid build (used by this
project's `esp32s3_usbhost` environment, see `../../platformio.ini`) did
not invoke the IDF Component Manager for a top-level `idf_component.yml`
during actual testing - the build failed with `usb/cdc_acm_host.h: No such
file or directory` instead of fetching it. Placing the component's source
directly under `components/` sidesteps that: PlatformIO's ESP-IDF build
automatically treats any folder here as an extra component, no component
manager step required.

## Why this copy's own `idf_component.yml` was deleted

That manifest declares `idf: ">=5.0"`, which the component's actual code
doesn't require for our target (the `esp32s3_usbhost` env builds against
IDF 4.4.7 - see the `usb` requirement logic in `CMakeLists.txt`, which
already handles pre-5.0 IDF by requiring the built-in `usb` component
instead of a managed one). Keeping the manifest around risked some future
component-manager pass enforcing that version constraint against a
component we're intentionally using outside the manager. `CMakeLists.txt`
alone is sufficient for PlatformIO to build this as a component.

## Updating

To update to a newer upstream release, replace every file in this
directory except this one and `idf_component.yml`'s absence is
intentional - re-delete it if it comes back in a refreshed copy - then
rebuild `esp32s3_usbhost` and fix any API differences in
`../../src/usb_host_link.cpp`.
