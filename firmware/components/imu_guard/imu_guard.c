#include "imu_guard.h"

#include <math.h>
#include <string.h>

#define GRAVITY_MPS2 9.81f

void imu_guard_init(imu_guard_t *g)
{
    memset(g, 0, sizeof(*g));
    g->retry_ms = IMU_GUARD_FIRST_RETRY_MS;
}

void imu_guard_restart(imu_guard_t *g)
{
    g->have_last = false;
    g->off = false;
    g->healthy = false;
}

bool imu_guard_is_stuck(const imu_guard_t *g)
{
    return g->stuck;
}

static bool elapsed(uint32_t now_ms, uint32_t since_ms, uint32_t span_ms)
{
    return (uint32_t)(now_ms - since_ms) >= span_ms;
}

imu_sample_verdict_t imu_guard_sample(imu_guard_t *g, float ax, float ay, float az,
                                      uint32_t now_ms)
{
    bool same = g->have_last && ax == g->last[0] && ay == g->last[1] && az == g->last[2];
    if (!same) {
        g->same_since_ms = now_ms;
    }
    g->have_last = true;
    g->last[0] = ax;
    g->last[1] = ay;
    g->last[2] = az;

    float mag = sqrtf(ax * ax + ay * ay + az * az);
    bool off = fabsf(mag - GRAVITY_MPS2) >= IMU_GUARD_OFF_MPS2;
    if (off && !g->off) {
        g->off_since_ms = now_ms;
    }
    g->off = off;

    if (!g->stuck) {
        bool frozen = same && elapsed(now_ms, g->same_since_ms, IMU_GUARD_FROZEN_MS);
        bool implausible = off && elapsed(now_ms, g->off_since_ms, IMU_GUARD_OFF_MS);
        if (!frozen && !implausible) {
            return IMU_SAMPLE_OK;
        }
        g->stuck = true;
        g->healthy = false;
        g->retry_ms = IMU_GUARD_FIRST_RETRY_MS;
        g->next_reset_ms = now_ms + g->retry_ms;
        return IMU_SAMPLE_RESET;
    }

    bool live = !same && !off;
    if (!live) {
        g->healthy = false;
    } else if (!g->healthy) {
        g->healthy = true;
        g->healthy_since_ms = now_ms;
    }
    if (g->healthy && elapsed(now_ms, g->healthy_since_ms, IMU_GUARD_HEALTHY_MS)) {
        g->stuck = false;
        g->healthy = false;
        return IMU_SAMPLE_OK;
    }

    if ((int32_t)(now_ms - g->next_reset_ms) >= 0) {
        g->retry_ms = g->retry_ms * 2 > IMU_GUARD_MAX_RETRY_MS ? IMU_GUARD_MAX_RETRY_MS
                                                                : g->retry_ms * 2;
        g->next_reset_ms = now_ms + g->retry_ms;
        return IMU_SAMPLE_RESET;
    }
    return IMU_SAMPLE_IGNORE;
}
