// Where is he? Works it out from the Wi-Fi networks a scan can see.
//
// A tube network (see place_tfl_ssids) puts him on his commute, and he stays
// there for PLACE_COMMUTE_HOLD_MS after the last one, because Wi-Fi is only in
// the stations, not the tunnels. Seeing the home network ends that early.
// Hardware-free, so the host tests can build it.
#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "pet.h"

#ifdef __cplusplus
extern "C" {
#endif

#define PLACE_COMMUTE_HOLD_MS (30u * 60u * 1000u)
// A home sighting goes stale after this (a bit over one 5-minute scan), so a
// stalled scanner hands back to the schedule.
#define PLACE_HOME_HOLD_MS (6u * 60u * 1000u)

typedef struct {
    const char *home_ssid;   // NULL or "" if unknown
    bool at_home;            // home network in the last scan
    uint32_t home_seen_ms;
    bool commuting;
    uint32_t tfl_seen_ms;    // last scan that saw a tube network
} place_t;

void place_init(place_t *place, const char *home_ssid);

// Feed one scan's network names (any order, duplicates fine).
void place_scan(place_t *place, const char *const *ssids, size_t count, uint32_t now_ms);

// Where he is now. `now_ms` is the same monotonic clock as place_scan().
pet_place_t place_now(const place_t *place, uint32_t now_ms);

// Is this one of the tube networks? Ignores case.
bool place_is_tfl_ssid(const char *ssid);

// The tube networks, NULL-terminated. Exposed for logging and tests.
extern const char *const place_tfl_ssids[];

#ifdef __cplusplus
}
#endif
