#include <stdio.h>

#include "imu_guard.h"

static int failures = 0;

#define CHECK(cond)                                                   \
    do {                                                              \
        if (!(cond)) {                                                \
            fprintf(stderr, "%s:%d: CHECK failed: %s\n", __FILE__,    \
                    __LINE__, #cond);                                 \
            failures++;                                               \
        }                                                             \
    } while (0)

#define STEP_MS 20u  // the firmware samples at 50 Hz
#define STUCK_MPS2 51.11f  // what the C6 board returned when it latched up

// A sensor at rest: about 1 g with a little noise, never identical twice.
static void rest(int i, float *x, float *y, float *z)
{
    *x = 0.01f * (float)(i % 7);
    *y = -0.02f * (float)(i % 5);
    *z = 9.81f + 0.005f * (float)(i % 3);
}

// Feed `ms` of samples starting at *t, counting each verdict.
typedef struct {
    int ok, ignore, reset;
} tally_t;

static tally_t feed_rest(imu_guard_t *g, uint32_t *t, uint32_t ms)
{
    tally_t n = {0};
    for (uint32_t i = 0; i < ms / STEP_MS; i++, *t += STEP_MS) {
        float x, y, z;
        rest((int)(*t / STEP_MS), &x, &y, &z);
        imu_sample_verdict_t v = imu_guard_sample(g, x, y, z, *t);
        n.ok += v == IMU_SAMPLE_OK;
        n.ignore += v == IMU_SAMPLE_IGNORE;
        n.reset += v == IMU_SAMPLE_RESET;
    }
    return n;
}

static tally_t feed_const(imu_guard_t *g, uint32_t *t, uint32_t ms, float x, float y, float z)
{
    tally_t n = {0};
    for (uint32_t i = 0; i < ms / STEP_MS; i++, *t += STEP_MS) {
        imu_sample_verdict_t v = imu_guard_sample(g, x, y, z, *t);
        n.ok += v == IMU_SAMPLE_OK;
        n.ignore += v == IMU_SAMPLE_IGNORE;
        n.reset += v == IMU_SAMPLE_RESET;
    }
    return n;
}

static void test_a_working_sensor_is_trusted(void)
{
    imu_guard_t g;
    imu_guard_init(&g);
    uint32_t t = 0;
    tally_t n = feed_rest(&g, &t, 60000);
    CHECK(n.ignore == 0 && n.reset == 0);
    CHECK(!imu_guard_is_stuck(&g));
}

static void test_a_hard_shake_is_trusted(void)
{
    imu_guard_t g;
    imu_guard_init(&g);
    uint32_t t = 0;
    tally_t n = {0};
    // Two seconds swinging between big jolts and back through 1 g.
    for (int i = 0; i < 100; i++, t += STEP_MS) {
        float z = (i % 4 == 0) ? 9.81f : (i % 2 ? 30.0f + i * 0.01f : -12.0f - i * 0.01f);
        n.ok += imu_guard_sample(&g, 0.1f * (float)(i % 3), 0.0f, z, t) == IMU_SAMPLE_OK;
    }
    CHECK(n.ok == 100);
}

static void test_frozen_readings_mean_stuck(void)
{
    imu_guard_t g;
    imu_guard_init(&g);
    uint32_t t = 0;
    feed_rest(&g, &t, 1000);
    tally_t n = feed_const(&g, &t, IMU_GUARD_FROZEN_MS + STEP_MS, 30.0f, 30.0f, 29.0f);
    CHECK(n.reset == 1);
    CHECK(imu_guard_is_stuck(&g));
    // Everything after that is dropped.
    n = feed_const(&g, &t, 500, 30.0f, 30.0f, 29.0f);
    CHECK(n.ok == 0);
}

static void test_frozen_at_one_g_is_stuck_too(void)
{
    imu_guard_t g;
    imu_guard_init(&g);
    uint32_t t = 0;
    feed_const(&g, &t, IMU_GUARD_FROZEN_MS + STEP_MS, 0.0f, 0.0f, 9.81f);
    CHECK(imu_guard_is_stuck(&g));
}

static void test_far_from_one_g_for_seconds_means_stuck(void)
{
    imu_guard_t g;
    imu_guard_init(&g);
    uint32_t t = 0;
    tally_t n = {0};
    // Jittery but never near 1 g: a broken sensor, not a shake.
    for (; t < IMU_GUARD_OFF_MS + 200; t += STEP_MS) {
        float j = 0.01f * (float)((t / STEP_MS) % 9);
        n.reset += imu_guard_sample(&g, 30.0f + j, 30.0f, 29.0f, t) == IMU_SAMPLE_RESET;
    }
    CHECK(n.reset == 1);
    CHECK(imu_guard_is_stuck(&g));
}

static void test_retries_back_off(void)
{
    imu_guard_t g;
    imu_guard_init(&g);
    uint32_t t = 0;
    feed_const(&g, &t, IMU_GUARD_FROZEN_MS, STUCK_MPS2, 0.0f, 0.0f);
    CHECK(imu_guard_sample(&g, STUCK_MPS2, 0.0f, 0.0f, t) == IMU_SAMPLE_RESET);
    t += STEP_MS;

    // Resets at +1s, +3s, +7s, +15s, ... capped at a minute apart.
    uint32_t last = t - STEP_MS;
    uint32_t gaps[12];
    int resets = 0;
    for (; t < 10u * 60000u && resets < 12; t += STEP_MS) {
        if (imu_guard_sample(&g, STUCK_MPS2, 0.0f, 0.0f, t) == IMU_SAMPLE_RESET) {
            gaps[resets++] = t - last;
            last = t;
        }
    }
    CHECK(resets == 12);
    CHECK(gaps[0] == 1000);
    CHECK(gaps[1] == 2000);
    CHECK(gaps[2] == 4000);
    CHECK(gaps[5] == 32000);
    CHECK(gaps[6] == IMU_GUARD_MAX_RETRY_MS);
    CHECK(gaps[11] == IMU_GUARD_MAX_RETRY_MS);
}

static void test_recovers_after_live_samples(void)
{
    imu_guard_t g;
    imu_guard_init(&g);
    uint32_t t = 0;
    feed_const(&g, &t, IMU_GUARD_FROZEN_MS + STEP_MS, STUCK_MPS2, 0.0f, 0.0f);
    CHECK(imu_guard_is_stuck(&g));

    // The reset worked: samples are live again, but trust takes a moment.
    tally_t n = feed_rest(&g, &t, IMU_GUARD_HEALTHY_MS);
    CHECK(n.ok == 0);
    n = feed_rest(&g, &t, 1000);
    CHECK(n.ok > 0);
    CHECK(!imu_guard_is_stuck(&g));

    // And the next latch-up starts the backoff from the beginning.
    feed_const(&g, &t, IMU_GUARD_FROZEN_MS + STEP_MS, STUCK_MPS2, 0.0f, 0.0f);
    CHECK(imu_guard_is_stuck(&g));
    CHECK(g.retry_ms == IMU_GUARD_FIRST_RETRY_MS);
}

static void test_a_shake_while_stuck_is_not_recovery(void)
{
    imu_guard_t g;
    imu_guard_init(&g);
    uint32_t t = 0;
    feed_const(&g, &t, IMU_GUARD_FROZEN_MS + STEP_MS, STUCK_MPS2, 0.0f, 0.0f);
    // Changing but always far from 1 g.
    for (int i = 0; i < 50; i++, t += STEP_MS) {
        imu_guard_sample(&g, 0.0f, 0.0f, 25.0f + (float)i, t);
    }
    CHECK(imu_guard_is_stuck(&g));
}

static void test_restart_forgets_stale_samples(void)
{
    imu_guard_t g;
    imu_guard_init(&g);
    uint32_t t = 0;
    CHECK(imu_guard_sample(&g, 0.0f, 0.0f, 9.81f, t) == IMU_SAMPLE_OK);
    // Paused for a minute (screen off), then the first read repeats the last.
    t += 60000;
    imu_guard_restart(&g);
    CHECK(imu_guard_sample(&g, 0.0f, 0.0f, 9.81f, t) == IMU_SAMPLE_OK);
    CHECK(!imu_guard_is_stuck(&g));
}

static void test_clock_wrap(void)
{
    imu_guard_t g;
    imu_guard_init(&g);
    uint32_t t = UINT32_MAX - 500;
    tally_t n = feed_rest(&g, &t, 2000);
    CHECK(n.ok == 100);
    feed_const(&g, &t, IMU_GUARD_FROZEN_MS + STEP_MS, STUCK_MPS2, 0.0f, 0.0f);
    CHECK(imu_guard_is_stuck(&g));
}

int main(void)
{
    test_a_working_sensor_is_trusted();
    test_a_hard_shake_is_trusted();
    test_frozen_readings_mean_stuck();
    test_frozen_at_one_g_is_stuck_too();
    test_far_from_one_g_for_seconds_means_stuck();
    test_retries_back_off();
    test_recovers_after_live_samples();
    test_a_shake_while_stuck_is_not_recovery();
    test_restart_forgets_stale_samples();
    test_clock_wrap();
    if (failures) {
        fprintf(stderr, "%d failure(s)\n", failures);
        return 1;
    }
    printf("imu_guard: all tests passed\n");
    return 0;
}
