#include "candle_flicker.h"

// A bit brighter than the flicker's own original intensity by default;
// see main.cpp's "brightness" serial command to adjust further.
float CandleFlicker::s_brightness = 1.15f;
