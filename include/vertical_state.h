#pragma once

#include <math.h>
#include <stdint.h>

namespace vertical_state {

enum class State : uint8_t {
  LEVEL,
  CLIMBING,
  DESCENDING
};

constexpr float ENTER_THRESHOLD_FPM = 200.0f;
constexpr float LEVEL_THRESHOLD_FPM = 100.0f;
constexpr float DISPLAY_INCREMENT_FPM = 50.0f;

inline State initialState(float rateFpm) {
  if (!isfinite(rateFpm)) return State::LEVEL;
  if (rateFpm >= ENTER_THRESHOLD_FPM) return State::CLIMBING;
  if (rateFpm <= -ENTER_THRESHOLD_FPM) return State::DESCENDING;
  return State::LEVEL;
}

inline State updateState(State current, float rateFpm) {
  if (!isfinite(rateFpm)) return State::LEVEL;

  switch (current) {
    case State::CLIMBING:
      if (rateFpm <= -ENTER_THRESHOLD_FPM) return State::DESCENDING;
      if (rateFpm <= LEVEL_THRESHOLD_FPM) return State::LEVEL;
      return State::CLIMBING;

    case State::DESCENDING:
      if (rateFpm >= ENTER_THRESHOLD_FPM) return State::CLIMBING;
      if (rateFpm >= -LEVEL_THRESHOLD_FPM) return State::LEVEL;
      return State::DESCENDING;

    case State::LEVEL:
    default:
      if (rateFpm >= ENTER_THRESHOLD_FPM) return State::CLIMBING;
      if (rateFpm <= -ENTER_THRESHOLD_FPM) return State::DESCENDING;
      return State::LEVEL;
  }
}

inline int roundedRateFpm(float rateFpm) {
  if (!isfinite(rateFpm)) return 0;
  return static_cast<int>(
      lroundf(rateFpm / DISPLAY_INCREMENT_FPM) * DISPLAY_INCREMENT_FPM);
}

inline const char* stateName(State state) {
  switch (state) {
    case State::CLIMBING:
      return "CLIMBING";
    case State::DESCENDING:
      return "DESCENDING";
    case State::LEVEL:
    default:
      return "LEVEL";
  }
}

}  // namespace vertical_state
