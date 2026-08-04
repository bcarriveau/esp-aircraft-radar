#pragma once

#include <stdint.h>

namespace adsb_transport_policy {

// The complete fetch stays below the fixed 15-second ADS-B cadence. Transport
// owns the first 10.5 seconds; the remaining 1.5 seconds is reserved for the
// existing PSRAM JSON parse and bounded aircraft extraction.
constexpr uint32_t FETCH_TOTAL_BUDGET_MS = 12000;
constexpr uint32_t JSON_RESERVE_MS = 1500;
constexpr uint32_t TRANSPORT_BUDGET_MS =
    FETCH_TOTAL_BUDGET_MS - JSON_RESERVE_MS;

// Stage ceilings are further reduced by the remaining shared transport budget.
constexpr uint32_t CONNECT_TIMEOUT_MS = 5000;
constexpr uint32_t HEADER_TIMEOUT_MS = 4000;
constexpr uint32_t BODY_READ_TIMEOUT_MS = 3000;
constexpr uint32_t IDLE_TIMEOUT_MS = 3000;
constexpr uint32_t TCP_PROBE_TIMEOUT_MS = 750;

constexpr uint32_t TRANSPORT_RELEASE_DELAY_MS = 250;
constexpr uint32_t RETRY_DELAY_MS = 500;
constexpr uint32_t FALLBACK_START_DELAY_MS = 250;
constexpr uint32_t MIN_NATIVE_RETRY_BUDGET_MS = 3500;
constexpr uint32_t MIN_FALLBACK_BUDGET_MS = 4000;
constexpr uint32_t MIN_BLOCKING_CALL_BUDGET_MS = 250;

constexpr uint32_t elapsedMs(uint32_t startedMs, uint32_t nowMs) {
  return nowMs - startedMs;
}

constexpr uint32_t remainingMs(uint32_t startedMs, uint32_t nowMs,
                               uint32_t budgetMs) {
  const uint32_t elapsed = elapsedMs(startedMs, nowMs);
  return elapsed >= budgetMs ? 0U : budgetMs - elapsed;
}

constexpr uint32_t transportRemainingMs(uint32_t startedMs,
                                        uint32_t nowMs) {
  return remainingMs(startedMs, nowMs, TRANSPORT_BUDGET_MS);
}

constexpr uint32_t fetchRemainingMs(uint32_t startedMs, uint32_t nowMs) {
  return remainingMs(startedMs, nowMs, FETCH_TOTAL_BUDGET_MS);
}

constexpr uint32_t boundedTimeoutMs(uint32_t remainingBudgetMs,
                                    uint32_t stageMaximumMs) {
  return remainingBudgetMs < stageMaximumMs ? remainingBudgetMs
                                             : stageMaximumMs;
}

constexpr bool canStartNativeRetry(uint32_t remainingBudgetMs) {
  return remainingBudgetMs >= MIN_NATIVE_RETRY_BUDGET_MS;
}

constexpr bool canStartFallback(uint32_t remainingBudgetMs) {
  return remainingBudgetMs >= MIN_FALLBACK_BUDGET_MS;
}

static_assert(JSON_RESERVE_MS < FETCH_TOTAL_BUDGET_MS,
              "JSON reserve must fit inside the total fetch budget");
static_assert(TRANSPORT_BUDGET_MS < FETCH_TOTAL_BUDGET_MS,
              "Transport must leave bounded JSON headroom");
static_assert(FETCH_TOTAL_BUDGET_MS < 15000,
              "Fetch budget must remain below the 15-second cadence");
static_assert(CONNECT_TIMEOUT_MS <= TRANSPORT_BUDGET_MS,
              "Connect timeout cannot exceed the shared transport budget");

}  // namespace adsb_transport_policy
