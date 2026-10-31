#include "motion.h"

#include <math.h>

#include "bsp/esp-bsp.h"
#include "driver/i2c_master.h"
#include "esp_check.h"
#include "esp_log.h"
#include "freertos/task.h"
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

static qmi8658_dev_t s_imu;
static QueueHandle_t s_events;
static TaskHandle_t s_task;
static volatile bool s_active = true;

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

    ESP_RETURN_ON_ERROR(qmi8658_init(&s_imu, bus, addr), TAG, "IMU setup");
    ESP_RETURN_ON_ERROR(qmi8658_set_accel_range(&s_imu, QMI8658_ACCEL_RANGE_4G), TAG, "IMU setup");
    ESP_RETURN_ON_ERROR(qmi8658_set_accel_odr(&s_imu, QMI8658_ACCEL_ODR_250HZ), TAG, "IMU setup");
    qmi8658_set_accel_unit_mps2(&s_imu, true);
    ESP_RETURN_ON_ERROR(qmi8658_enable_sensors(&s_imu, QMI8658_ENABLE_ACCEL), TAG, "IMU setup");

    ESP_LOGI(TAG, "IMU ready at 0x%02x", addr);
    return ESP_OK;
}

static void motion_task(void *arg)
{
    (void)arg;
    int jolts = 0;
    TickType_t window_start = 0;
    TickType_t cooldown_until = 0;

    for (;;) {
        if (!s_active) {
            ulTaskNotifyTake(pdTRUE, portMAX_DELAY);  // until motion_set_active(true)
            continue;
        }
        vTaskDelay(pdMS_TO_TICKS(SAMPLE_PERIOD_MS));

        qmi8658_data_t d = {0};
        if (qmi8658_read_sensor_data(&s_imu, &d) != ESP_OK) {
            continue;
        }

        TickType_t now = xTaskGetTickCount();
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
