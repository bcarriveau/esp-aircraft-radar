#pragma once

#include <stdint.h>

// Lightweight sprite metadata shared by aircraft helpers and radar drawing.
constexpr uint16_t AIRCRAFT_BITMAP_W = 96;
constexpr uint16_t AIRCRAFT_BITMAP_H = 64;

enum class AircraftBitmapId : uint8_t {
  AIRLINER,
  BUSINESS_JET,
  TURBOPROP,
  PISTON,
  HELICOPTER,
  UNKNOWN
};
