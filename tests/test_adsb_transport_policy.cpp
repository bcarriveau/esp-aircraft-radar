#include <cassert>
#include <cstdint>

#include "adsb_transport_policy.h"

int main() {
  using namespace adsb_transport_policy;

  static_assert(FETCH_TOTAL_BUDGET_MS == 12000);
  static_assert(TRANSPORT_BUDGET_MS == 10500);
  static_assert(FETCH_TOTAL_BUDGET_MS < 15000);
  static_assert(CONNECT_TIMEOUT_MS <= TRANSPORT_BUDGET_MS);

  assert(transportRemainingMs(1000, 1000) == 10500);
  assert(transportRemainingMs(1000, 6000) == 5500);
  assert(transportRemainingMs(1000, 11500) == 0);
  assert(fetchRemainingMs(1000, 12999) == 1);
  assert(fetchRemainingMs(1000, 13000) == 0);

  assert(boundedTimeoutMs(9000, CONNECT_TIMEOUT_MS) == CONNECT_TIMEOUT_MS);
  assert(boundedTimeoutMs(1200, CONNECT_TIMEOUT_MS) == 1200);
  assert(canStartNativeRetry(MIN_NATIVE_RETRY_BUDGET_MS));
  assert(!canStartNativeRetry(MIN_NATIVE_RETRY_BUDGET_MS - 1));
  assert(canStartFallback(MIN_FALLBACK_BUDGET_MS));
  assert(!canStartFallback(MIN_FALLBACK_BUDGET_MS - 1));

  // Unsigned subtraction keeps short elapsed intervals valid across millis()
  // wrap-around.
  const uint32_t nearWrap = UINT32_MAX - 99U;
  assert(elapsedMs(nearWrap, 50U) == 150U);
  assert(remainingMs(nearWrap, 50U, 1000U) == 850U);

  return 0;
}
