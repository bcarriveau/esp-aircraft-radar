#!/usr/bin/env python3
"""Focused checks for Product 91 verified airport auto-restart and UI wording."""

from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
OTA = (ROOT / "src" / "ota_update.cpp").read_text(encoding="utf-8")
BUILD = (ROOT / "include" / "build_info.h").read_text(encoding="utf-8")


def require(text: str, needle: str) -> None:
    assert needle in text, f"missing Product 91 requirement: {needle}"


def main() -> None:
    require(BUILD, "FIRMWARE_VERSION_CODE = 91")
    require(BUILD, "PRODUCT91-AIRPORT-AUTO-RESTART")

    require(OTA, "BACK TO FIRMWARE UPDATE")
    assert '>FIRMWARE UPDATE</a>' not in OTA

    install_call = OTA.index("const bool installed = airport_store::installPackage(")
    success_response = OTA.index(
        '200, "Airport database verified and installed. Radar is restarting."'
    )
    success_state = OTA.index("currentState = State::SUCCESS;", success_response)
    restart_arm = OTA.index("restartAtMs = millis() + RESTART_DELAY_MS;", success_response)
    assert install_call < success_response < success_state < restart_arm

    # The existing hardened restart machinery must remain the implementation.
    require(OTA, "if (restartAtMs && (int32_t)(now - restartAtMs) >= 0)")
    require(OTA, "restartExecuteAtMs = now + RESTART_SETTLE_MS;")
    require(OTA, "createRestartTaskOnce();")
    require(OTA, "parkCoreOneForRestart()")
    require(OTA, "xTaskCreatePinnedToCoreWithCaps(")

    # Product 90 upload timing/retry hardening must remain intact.
    require(
        OTA,
        "Radar ready. Settling web connection before upload..."
    )
    require(
        OTA,
        "Upload connection reset before transfer; retrying once..."
    )
    require(OTA, "if(ota.state!=='READY'||received!==0)")

    # No direct/simple restart was introduced for airport install.
    assert "ESP.restart()" not in OTA

    require(
        OTA,
        "After verification the radar restarts automatically and activates the new region."
    )
    assert "Restart the radar after a successful install" not in OTA

    print("Product 91 airport auto-restart/UI checks passed")


if __name__ == "__main__":
    main()
