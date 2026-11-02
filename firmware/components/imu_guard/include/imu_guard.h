// Spots a stuck motion sensor and says when to reset it.
//
// On a flat battery the QMI8658 can latch up: reads still succeed but return
// the same sample forever (seen: |a| = 51 m/s^2 at rest), which looks like a
// shake every time. Resetting the ESP32 doesn't fix it because the IMU stays
// powered, so the firmware has to soft-reset the IMU itself.
//
// The sensor counts as stuck after IMU_GUARD_FROZEN_MS of identical samples
// (real readings always jitter), or IMU_GUARD_OFF_MS where every sample is far
// from 1 g (a real shake keeps passing back through 1 g). While stuck, samples
// are ignored and the firmware is asked to reset the IMU, retrying with
// backoff. It counts as working again after IMU_GUARD_HEALTHY_MS of changing,
// plausible samples.
//
// Pure logic: `now_ms` is a monotonic clock that may wrap.
#pragma once

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define IMU_GUARD_FROZEN_MS 1000u
#define IMU_GUARD_OFF_MS 3000u
#define IMU_GUARD_OFF_MPS2 8.0f          // how far from 1 g counts as "far"
#define IMU_GUARD_HEALTHY_MS 500u
#define IMU_GUARD_FIRST_RETRY_MS 1000u
#define IMU_GUARD_MAX_RETRY_MS 60000u

typedef enum {
    IMU_SAMPLE_OK,      // use the sample
    IMU_SAMPLE_IGNORE,  // the sensor is stuck: drop the sample
    IMU_SAMPLE_RESET,   // the sensor is stuck: drop the sample and reset the IMU now
} imu_sample_verdict_t;

typedef struct {
    bool have_last;
    float last[3];
    uint32_t same_since_ms;  // first sample of the current run of identical ones
    bool off;                // the latest sample was far from 1 g
    uint32_t off_since_ms;
    bool stuck;
    bool healthy;            // while stuck: the latest sample looked live
    uint32_t healthy_since_ms;
    uint32_t next_reset_ms;
    uint32_t retry_ms;
} imu_guard_t;

void imu_guard_init(imu_guard_t *g);

// Forget the sample history but not whether the sensor is stuck. Call it when
// sampling resumes after a pause, so stale readings don't look frozen.
void imu_guard_restart(imu_guard_t *g);

// Check one accelerometer sample (m/s^2).
imu_sample_verdict_t imu_guard_sample(imu_guard_t *g, float ax, float ay, float az,
                                      uint32_t now_ms);

bool imu_guard_is_stuck(const imu_guard_t *g);

#ifdef __cplusplus
}
#endif
