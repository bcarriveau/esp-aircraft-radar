#!/usr/bin/env python3
"""Product 64 checks retaining Product 62 airport eye coverage."""

from dataclasses import dataclass
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
DIRECTORY_CAPACITY = 64
MAX_NEARBY_AIRPORTS = 192


@dataclass(frozen=True)
class Airport:
    ident: str
    distance: float


def retain_visible_airports(
    nearby: list[Airport], visible_idents: set[str]
) -> list[Airport]:
    """Model the bounded directory-retention policy implemented in ui.cpp."""
    retained = list(nearby[:DIRECTORY_CAPACITY])
    visible = [airport.ident in visible_idents for airport in retained]

    for candidate in nearby[DIRECTORY_CAPACITY:MAX_NEARBY_AIRPORTS]:
        if candidate.ident not in visible_idents:
            continue
        if any(airport.ident == candidate.ident for airport in retained):
            continue
        replaceable = [
            index for index, is_visible in enumerate(visible) if not is_visible
        ]
        if not replaceable:
            break
        replace_at = max(replaceable, key=lambda index: retained[index].distance)
        retained[replace_at] = candidate
        visible[replace_at] = True

    return sorted(retained, key=lambda airport: airport.distance)


def require(text: str, needle: str, context: str) -> None:
    assert needle in text, f"missing {context}: {needle}"


def main() -> None:
    nearby = [Airport(f"K{index:03d}", float(index)) for index in range(120)]
    visible_idents = {
        "K002", "K017", "K033", "K070", "K088", "K105", "K119"
    }
    retained = retain_visible_airports(nearby, visible_idents)

    assert len(retained) == DIRECTORY_CAPACITY
    assert retained == sorted(retained, key=lambda airport: airport.distance)
    retained_idents = {airport.ident for airport in retained}
    assert visible_idents.issubset(retained_idents)
    assert len(visible_idents & retained_idents) == 7
    assert {"K061", "K062", "K063"}.isdisjoint(
        {airport.ident for airport in retained}
    ), "farthest non-visible rows should make room for visible radar labels"

    ui = (ROOT / "src" / "ui.cpp").read_text(encoding="utf-8")
    build = (ROOT / "include" / "build_info.h").read_text(encoding="utf-8")

    require(
        build,
        "7IN-20260802-PRODUCT67-RADAR-GAP-ATTRIBUTION",
        "Product 66 build marker",
    )
    require(ui, "AIRPORT_DIRECTORY_CAPACITY = 64", "bounded directory capacity")
    require(
        ui,
        "airport_data::NearbyAirport* airportDirectoryEntries = nullptr",
        "PSRAM-backed directory entries",
    )
    require(
        ui,
        "airport_data::NearbyAirport* airportDirectoryScratch = nullptr",
        "full nearby-set PSRAM scratch",
    )
    require(
        ui,
        "airport_data::MAX_NEARBY_AIRPORTS",
        "shared nearby-airport capacity",
    )
    require(ui, "MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT", "PSRAM allocation")
    require(ui, "farthestReplaceableAirportRow()", "bounded replacement policy")
    require(ui, "airportDirectoryContains(", "stable-ident duplicate protection")
    require(ui, "sortAirportDirectoryByDistance()", "distance-order restoration")
    require(
        ui,
        "airportDirectoryEntries[replaceRow] = airportDirectoryScratch[candidate]",
        "visible airport retention",
    )
    require(
        ui,
        "airportDirectoryLabelVisible[replaceRow] = true",
        "retained eye state",
    )
    require(
        ui,
        "Airport directory scratch unavailable; using nearest rows only",
        "bounded scratch fallback",
    )
    require(
        ui,
        "Airport directory entries unavailable; directory page disabled",
        "nonfatal directory-storage failure",
    )

    update = ui[ui.index("void updateAirportDirectory()") : ui.index(
        "void showAirportDirectory()"
    )]
    assert "uint8_t candidate" not in update
    assert "uint8_t row" not in update
    assert "HTTPClient::GET" not in ui
    assert "setInsecure" not in ui

    print("Product 64 airport eye-coverage checks passed")


if __name__ == "__main__":
    main()
