from __future__ import annotations

import subprocess
import tempfile
import textwrap
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


class UpdatePolicyHostTests(unittest.TestCase):
    def test_actual_policy_header_under_sanitizers(self) -> None:
        source = r'''
#include <assert.h>
#include <stdint.h>
#include <string.h>
#include <string>
#include "update_policy.h"

int main() {
  using namespace update_policy;
  assert(compareVersion(0, 72) == VersionRelation::INVALID);
  assert(compareVersion(71, 72) == VersionRelation::OLDER);
  assert(compareVersion(72, 72) == VersionRelation::CURRENT);
  assert(compareVersion(73, 72) == VersionRelation::NEWER);

  char digest[65];
  memset(digest, 'a', 64);
  digest[64] = 0;
  assert(lowerHexDigest(digest));
  digest[12] = 'G';
  assert(!lowerHexDigest(digest));
  digest[12] = 'a';
  digest[63] = 0;
  assert(!lowerHexDigest(digest));

  assert(identityMatches(1, 1, "waveshare-esp32-s3-touch-lcd-7",
                         "waveshare-esp32-s3-touch-lcd-7", "stable",
                         "stable", 1, 1));
  assert(!identityMatches(1, 1, "other-hardware",
                          "waveshare-esp32-s3-touch-lcd-7", "stable",
                          "stable", 1, 1));
  assert(!identityMatches(1, 1, "waveshare-esp32-s3-touch-lcd-7",
                          "waveshare-esp32-s3-touch-lcd-7", "beta",
                          "stable", 1, 1));
  assert(!identityMatches(1, 1, "waveshare-esp32-s3-touch-lcd-7",
                          "waveshare-esp32-s3-touch-lcd-7", "stable",
                          "stable", 2, 1));

  assert(packageLayoutValid(64U * 1024U + 512U, 64U * 1024U));
  assert(!packageLayoutValid(64U * 1024U + 511U, 64U * 1024U));
  assert(!packageLayoutValid(8U * 1024U * 1024U + 1U,
                             8U * 1024U * 1024U - 511U));
  assert(assetNameValid("waveshare-esp32-s3-touch-lcd-7-product-73.radarota"));
  assert(!assetNameValid("firmware.bin"));

  char host[96];
  assert(parseAllowedHttpsUrl(
      "https://github.com/bcarriveau/esp-aircraft-radar/releases/latest", host,
      sizeof(host)));
  assert(strcmp(host, "github.com") == 0);
  assert(parseAllowedHttpsUrl(
      "https://RELEASE-ASSETS.GITHUBUSERCONTENT.COM/path", host,
      sizeof(host)));
  assert(!parseAllowedHttpsUrl("http://github.com/path", host, sizeof(host)));
  assert(!parseAllowedHttpsUrl("https://github.com:443/path", host,
                               sizeof(host)));
  assert(!parseAllowedHttpsUrl("https://user@github.com/path", host,
                               sizeof(host)));
  assert(!parseAllowedHttpsUrl("https://example.com/path", host, sizeof(host)));

  size_t headerTotal = 0;
  for (int index = 0; index < 48; ++index) {
    size_t updated = 0;
    assert(accumulateHeaderBytes(headerTotal, 24, 180, updated));
    headerTotal = updated;
  }
  assert(headerTotal > 4096U);
  assert(headerTotal < MAX_HTTP_HEADER_BYTES);
  size_t rejectedTotal = 0;
  assert(!accumulateHeaderBytes(MAX_HTTP_HEADER_BYTES - 2U, 1U, 1U,
                                rejectedTotal));

  std::string signedRedirect =
      "https://release-assets.githubusercontent.com/github-production-release-asset/";
  signedRedirect.append(1800U, 'a');
  signedRedirect += "?sp=r&sv=2025-01-05&sr=b&spr=https";
  assert(signedRedirect.size() > 1024U);
  assert(redirectUrlLengthValid(signedRedirect.c_str()));
  assert(parseAllowedHttpsUrl(signedRedirect.c_str(), host, sizeof(host)));
  std::string oversizedRedirect(MAX_REDIRECT_URL_LENGTH + 1U, 'a');
  assert(!redirectUrlLengthValid(oversizedRedirect.c_str()));

  assert(httpTransmitBufferBytes(0) == MIN_HTTP_TX_BUFFER_BYTES);
  assert(httpTransmitBufferBytes(100) == MIN_HTTP_TX_BUFFER_BYTES);
  assert(httpTransmitBufferBytes(512) == MIN_HTTP_TX_BUFFER_BYTES);
  assert(httpTransmitBufferBytes(513) == 1025U);
  assert(httpTransmitBufferBytes(1800) == 2312U);
  assert(httpTransmitBufferBytes(MAX_REDIRECT_URL_LENGTH) ==
         MAX_HTTP_TX_BUFFER_BYTES);
  assert(httpTransmitBufferBytes(MAX_REDIRECT_URL_LENGTH + 1U) == 0);
  assert(MAX_HTTP_TX_BUFFER_BYTES == 4607U);

  assert(framingIsUnambiguous(true, false, false, false));
  assert(framingIsUnambiguous(false, true, true, true));
  assert(!framingIsUnambiguous(true, true, true, true));
  assert(!framingIsUnambiguous(false, false, false, false));
  assert(!framingIsUnambiguous(false, true, false, true));

  assert(enoughSlack(1000, 9000, 8000));
  assert(!enoughSlack(1000, 8999, 8000));
  assert(boundedDeadline(1000, 10000, 6000, 1500) == 7000);
  assert(boundedDeadline(1000, 7500, 6000, 1500) == 6000);
  return 0;
}
'''
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            cpp = root / "update_policy_test.cpp"
            exe = root / "update_policy_test"
            cpp.write_text(textwrap.dedent(source), encoding="utf-8")
            compile_result = subprocess.run(
                [
                    "g++", "-std=c++17", "-Wall", "-Wextra", "-Werror",
                    "-fsanitize=address,undefined", "-fno-omit-frame-pointer",
                    "-I", str(ROOT / "include"), str(cpp), "-o", str(exe),
                ],
                text=True,
                capture_output=True,
                check=False,
            )
            self.assertEqual(compile_result.returncode, 0, compile_result.stderr)
            run_result = subprocess.run(
                [str(exe)], text=True, capture_output=True, check=False
            )
            self.assertEqual(run_result.returncode, 0, run_result.stderr)


if __name__ == "__main__":
    unittest.main()
