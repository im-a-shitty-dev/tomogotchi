#include "motion.h"

#include <math.h>

#include "bsp/esp-bsp.h"
#include "driver/i2c_master.h"
#include "esp_check.h"
#include "esp_log.h"
#include "freertos/task.h"
#include "imu_guard.h"
#include "pet.h"
#include "qmi8658.h"

static const char *TAG = "motion";

// The S3 board package doesn't name the IMU's address; we probe both anyway.
#ifndef BSP_IMU_I2C_ADDRESS
#define BSP_IMU_I2C_ADDRESS 0x6B
#endif

#define SAMPLE_PERIOD_MS 20      // 50 Hz
#define GRAVITY_MPS2 9.81f
#define SHAKE_THRESHOLD_MPS2 8.0f // deviation from 1 g that counts as a jolt
#define SHAKE_JOLTS 3            // jolts needed inside the window
#define SHAKE_WINDOW_MS 600
#define SHAKE_COOLDOWN_MS 1500

// Not in the driver's register list. Its qmi8658_reset() writes CTRL1, which
// isn't a reset, so we do the real one ourselves.
#define QMI8658_REG_RESET 0x60
#define QMI8658_RESET_CMD 0xB0
#define QMI8658_REG_RESET_RESULT 0x4D  // reads 0x80 once the reset is done
#define QMI8658_RESET_DONE 0x80
#define QMI8658_WHO_AM_I_VALUE 0x05

static qmi8658_dev_t s_imu;
static QueueHandle_t s_events;
static TaskHandle_t s_task;
static volatile bool s_active = true;

// Soft-reset the IMU. It keeps running from the battery when the ESP32
// resets, so a bad state (frozen readings) survives a reflash unless we
// clear it.
static esp_err_t imu_soft_reset(void)
{
    ESP_RETURN_ON_ERROR(qmi8658_write_register(&s_imu, QMI8658_REG_RESET, QMI8658_RESET_CMD),
                        TAG, "IMU reset");
    // Takes ~15ms. The chip may not answer while it reboots, so ignore errors.
    uint8_t done = 0;
    for (int i = 0; i < 10 && done != QMI8658_RESET_DONE; i++) {
        vTaskDelay(pdMS_TO_TICKS(10));
        if (qmi8658_read_register(&s_imu, QMI8658_REG_RESET_RESULT, &done, 1) != ESP_OK) {
            done = 0;
        }
    }
    if (done != QMI8658_RESET_DONE) {
        ESP_LOGW(TAG, "IMU reset didn't report done (0x%02x)", done);
    }
    uint8_t who = 0;
    ESP_RETURN_ON_ERROR(qmi8658_get_who_am_i(&s_imu, &who), TAG, "IMU reset");
    if (who != QMI8658_WHO_AM_I_VALUE) {
        ESP_LOGE(TAG, "IMU after reset: WHO_AM_I 0x%02x", who);
        return ESP_ERR_INVALID_RESPONSE;
    }
    return ESP_OK;
}

// Set the IMU up from scratch. Range and rate are changed with the sensors
// off, as the datasheet recommends.
static esp_err_t imu_configure(void)
{
    ESP_RETURN_ON_ERROR(imu_soft_reset(), TAG, "IMU setup");
    // Auto-increment addresses for burst reads, same as the driver's init.
    ESP_RETURN_ON_ERROR(qmi8658_write_register(&s_imu, QMI8658_CTRL1, 0x60), TAG, "IMU setup");
    ESP_RETURN_ON_ERROR(qmi8658_enable_sensors(&s_imu, QMI8658_DISABLE_ALL), TAG, "IMU setup");
    ESP_RETURN_ON_ERROR(qmi8658_set_accel_range(&s_imu, QMI8658_ACCEL_RANGE_4G), TAG, "IMU setup");
    ESP_RETURN_ON_ERROR(qmi8658_set_accel_odr(&s_imu, QMI8658_ACCEL_ODR_250HZ), TAG, "IMU setup");
    qmi8658_set_accel_unit_mps2(&s_imu, true);
    if (s_active) {
        ESP_RETURN_ON_ERROR(qmi8658_enable_sensors(&s_imu, QMI8658_ENABLE_ACCEL), TAG, "IMU setup");
    }
    return ESP_OK;
}

// Log the registers that say whether the IMU is sampling, for diagnosis.
static void imu_log_state(void)
{
    uint8_t ctrl[8] = {0};  // CTRL1..CTRL8
    uint8_t status[2] = {0};
    uint8_t ts[3] = {0};
    qmi8658_read_register(&s_imu, QMI8658_CTRL1, ctrl, sizeof(ctrl));
    qmi8658_read_register(&s_imu, QMI8658_STATUS0, status, sizeof(status));
    qmi8658_read_register(&s_imu, QMI8658_TIMESTAMP_L, ts, sizeof(ts));
    ESP_LOGW(TAG, "IMU regs: CTRL1=%02x CTRL2=%02x CTRL7=%02x STATUS0=%02x STATUS1=%02x ts=%lu",
             ctrl[0], ctrl[1], ctrl[6], status[0], status[1],
             (unsigned long)(ts[0] | (ts[1] << 8) | ((uint32_t)ts[2] << 16)));
}

static esp_err_t imu_init(void)
{
    i2c_master_bus_handle_t bus = bsp_i2c_get_handle();
    if (bus == NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    const uint8_t candidates[] = {BSP_IMU_I2C_ADDRESS, QMI8658_ADDRESS_LOW};
    uint8_t addr = 0;
    for (size_t i = 0; i < sizeof(candidates); i++) {
        if (i2c_master_probe(bus, candidates[i], 100) == ESP_OK) {
            addr = candidates[i];
            break;
        }
    }
    if (addr == 0) {
        return ESP_ERR_NOT_FOUND;
    }

    // Adds the device to the bus (once) and checks WHO_AM_I.
    ESP_RETURN_ON_ERROR(qmi8658_init(&s_imu, bus, addr), TAG, "IMU setup");
    ESP_RETURN_ON_ERROR(imu_configure(), TAG, "IMU setup");

    ESP_LOGI(TAG, "IMU ready at 0x%02x", addr);
    return ESP_OK;
}

static void motion_task(void *arg)
{
    (void)arg;
    int jolts = 0;
    TickType_t window_start = 0;
    TickType_t cooldown_until = 0;
    imu_guard_t guard;
    imu_guard_init(&guard);

    for (;;) {
        if (!s_active) {
            ulTaskNotifyTake(pdTRUE, portMAX_DELAY);  // until motion_set_active(true)
            imu_guard_restart(&guard);  // the last reading before the pause is stale
            jolts = 0;
            continue;
        }
        vTaskDelay(pdMS_TO_TICKS(SAMPLE_PERIOD_MS));

        qmi8658_data_t d = {0};
        if (qmi8658_read_sensor_data(&s_imu, &d) != ESP_OK) {
            continue;
        }

        TickType_t now = xTaskGetTickCount();
        bool was_stuck = imu_guard_is_stuck(&guard);
        imu_sample_verdict_t verdict =
            imu_guard_sample(&guard, d.accelX, d.accelY, d.accelZ, pdTICKS_TO_MS(now));
        if (verdict == IMU_SAMPLE_RESET) {
            if (!was_stuck) {
                ESP_LOGW(TAG, "IMU looks stuck (%.2f, %.2f, %.2f): resetting it", d.accelX,
                         d.accelY, d.accelZ);
                imu_log_state();
            }
            esp_err_t err = imu_configure();
            if (err != ESP_OK) {
                ESP_LOGE(TAG, "IMU reset failed: %s", esp_err_to_name(err));
            }
            imu_guard_restart(&guard);
        }
        if (verdict != IMU_SAMPLE_OK) {
            jolts = 0;
            continue;
        }
        if (was_stuck) {
            ESP_LOGI(TAG, "IMU working again");
        }
        if ((int32_t)(now - cooldown_until) < 0) {
            continue;
        }

        float mag = sqrtf(d.accelX * d.accelX + d.accelY * d.accelY + d.accelZ * d.accelZ);
        if (fabsf(mag - GRAVITY_MPS2) < SHAKE_THRESHOLD_MPS2) {
            continue;
        }

        if (jolts == 0 || (now - window_start) > pdMS_TO_TICKS(SHAKE_WINDOW_MS)) {
            jolts = 0;
            window_start = now;
        }
        if (++jolts >= SHAKE_JOLTS) {
            pet_event_t ev = {.type = PET_INPUT_SHAKE};
            xQueueSend(s_events, &ev, 0);
            jolts = 0;
            cooldown_until = now + pdMS_TO_TICKS(SHAKE_COOLDOWN_MS);
        }
    }
}

esp_err_t motion_start(QueueHandle_t events)
{
    s_events = events;

    esp_err_t err = imu_init();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "IMU init failed: %s", esp_err_to_name(err));
        return err;
    }

    BaseType_t ok = xTaskCreate(motion_task, "motion", 3072, NULL, 4, &s_task);
    return ok == pdPASS ? ESP_OK : ESP_ERR_NO_MEM;
}

void motion_set_active(bool active)
{
    if (s_task == NULL || active == s_active) {
        return;
    }
    s_active = active;
    // Powered down, the sensor draws next to nothing.
    qmi8658_enable_sensors(&s_imu, active ? QMI8658_ENABLE_ACCEL : QMI8658_DISABLE_ALL);
    if (active) {
        xTaskNotifyGive(s_task);
    }
}
