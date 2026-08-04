#!/usr/bin/env python3
from pathlib import Path
import shutil
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
FETCH = (ROOT / "src" / "adsb_fetch.cpp").read_text(encoding="utf-8")
NETWORK = (ROOT / "src" / "adsb_network.cpp").read_text(encoding="utf-8")
FETCH_H = (ROOT / "include" / "adsb_fetch.h").read_text(encoding="utf-8")
NETWORK_H = (ROOT / "include" / "adsb_network.h").read_text(encoding="utf-8")
POLICY = (ROOT / "include" / "adsb_transport_policy.h").read_text(encoding="utf-8")
BUILD = (ROOT / "include" / "build_info.h").read_text(encoding="utf-8")

assert "7IN-20260803-PRODUCT69-BOUNDED-TRANSPORT" in BUILD
assert "FETCH_TOTAL_BUDGET_MS = 12000" in POLICY
assert "TRANSPORT_BUDGET_MS" in POLICY
assert "FETCH_TOTAL_BUDGET_MS < 15000" in POLICY

assert '#include "adsb_transport_policy.h"' in FETCH
assert "MAX_NATIVE_ATTEMPTS = 2" in FETCH
assert "esp_http_client_open" in FETCH
assert "WiFiClientSecure" in FETCH
assert "esp_crt_bundle_attach" in FETCH
assert "skip_cert_common_name_check = false" in FETCH
assert "MALLOC_CAP_SPIRAM" in FETCH
assert "MAX_RESPONSE_BYTES = 250000" in FETCH
assert "fetchAbortRequested" in NETWORK_H
assert "bool fetchAbortRequested()" in NETWORK
assert "result.cancelled" in NETWORK
assert "transportBudgetExhausted" in FETCH_H

for forbidden in (
    "HTTPClient",
    "setInsecure",
    "TOTAL_TIMEOUT_MS = 45000",
    "HTTP_NETWORK_TIMEOUT_MS = 15000",
):
    assert forbidden not in FETCH, forbidden

compiler = shutil.which("g++") or shutil.which("clang++")
if compiler:
    with tempfile.TemporaryDirectory() as temp_dir:
        binary = Path(temp_dir) / "policy_test"
        subprocess.run(
            [
                compiler,
                "-std=c++17",
                "-Wall",
                "-Wextra",
                "-Werror",
                "-fsanitize=address,undefined",
                "-I",
                str(ROOT / "include"),
                str(ROOT / "tests" / "test_adsb_transport_policy.cpp"),
                "-o",
                str(binary),
            ],
            check=True,
        )
        subprocess.run([str(binary)], check=True)

print("Product 69 focused source and transport-policy checks passed")
