#include "place.h"

#include <ctype.h>

// Wi-Fi seen in London Underground stations (carrier hotspots). Skip
// The_Cloud and the like: they're in pubs and shops too. Add more as we
// spot them on the device's scan log.
const char *const place_tfl_ssids[] = {
    "THREE_WIFI",
    "EE WiFi-Auto",
    "Wifi Extra",
    "WiFi_Extra",
    "VodafoneWiFi",
    "Vodafone_WiFi",
    "Virgin Media WiFi",
    // "Rail Net Police": British Transport Police kit, reportedly at every
    // tube stop with Wi-Fi. Seen written with both an l and an I.
    "Ra1ln3tP0l",
    "Ra1In3tP0l",
    NULL,
};

static bool same_ignoring_case(const char *a, const char *b)
{
    for (; *a && *b; a++, b++) {
        if (tolower((unsigned char)*a) != tolower((unsigned char)*b)) {
            return false;
        }
    }
    return *a == *b;
}

bool place_is_tfl_ssid(const char *ssid)
{
    for (const char *const *t = place_tfl_ssids; *t; t++) {
        if (same_ignoring_case(ssid, *t)) {
            return true;
        }
    }
    return false;
}

void place_init(place_t *place, const char *home_ssid)
{
    *place = (place_t){.home_ssid = home_ssid};
}

void place_scan(place_t *place, const char *const *ssids, size_t count, uint32_t now_ms)
{
    bool home = false;
    bool tfl = false;
    for (size_t i = 0; i < count; i++) {
        if (!ssids[i]) continue;
        if (place->home_ssid && place->home_ssid[0] && same_ignoring_case(ssids[i], place->home_ssid)) {
            home = true;
        }
        if (place_is_tfl_ssid(ssids[i])) {
            tfl = true;
        }
    }
    place->at_home = home;
    if (home) {
        place->home_seen_ms = now_ms;
        place->commuting = false;  // home wins, even if a hotspot is in range
    } else if (tfl) {
        place->commuting = true;
        place->tfl_seen_ms = now_ms;
    }
}

pet_place_t place_now(const place_t *place, uint32_t now_ms)
{
    if (place->at_home && (uint32_t)(now_ms - place->home_seen_ms) < PLACE_HOME_HOLD_MS) {
        return PET_PLACE_HOME;
    }
    if (place->commuting && (uint32_t)(now_ms - place->tfl_seen_ms) < PLACE_COMMUTE_HOLD_MS) {
        return PET_PLACE_COMMUTING;
    }
    return PET_PLACE_UNKNOWN;
}
