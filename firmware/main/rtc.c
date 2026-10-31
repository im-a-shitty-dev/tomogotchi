#include "rtc.h"

#include "bsp/esp-bsp.h"
#include "driver/i2c_master.h"
#include "esp_log.h"

static const char *TAG = "rtc";

#define PCF85063_ADDR 0x51
#define REG_SECONDS 0x04  // then minutes, hours, days, weekdays, months, years
#define OS_FLAG 0x80      // in seconds: oscillator stopped, time is junk
#define I2C_TIMEOUT_MS 100

static i2c_master_dev_handle_t s_dev;

static int from_bcd(uint8_t v) { return (v >> 4) * 10 + (v & 0x0f); }
static uint8_t to_bcd(int v) { return (uint8_t)(((v / 10) << 4) | (v % 10)); }

// Days since 1970-01-01 for a proleptic Gregorian date (Howard Hinnant's
// days_from_civil). newlib has no timegm(), and mktime() would apply our TZ.
static long days_from_civil(int y, int m, int d)
{
    y -= m <= 2;
    long era = (y >= 0 ? y : y - 399) / 400;
    long yoe = y - era * 400;
    long doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
    long doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
    return era * 146097 + doe - 719468;
}

esp_err_t rtc_start(void)
{
    i2c_master_bus_handle_t bus = bsp_i2c_get_handle();
    if (!bus) {
        return ESP_ERR_INVALID_STATE;
    }
    esp_err_t err = i2c_master_probe(bus, PCF85063_ADDR, I2C_TIMEOUT_MS);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "no PCF85063 at 0x%02x", PCF85063_ADDR);
        return err;
    }
    i2c_device_config_t cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = PCF85063_ADDR,
        .scl_speed_hz = 400000,
    };
    return i2c_master_bus_add_device(bus, &cfg, &s_dev);
}

esp_err_t rtc_read(time_t *utc)
{
    if (!s_dev) {
        return ESP_ERR_INVALID_STATE;
    }
    uint8_t reg = REG_SECONDS;
    uint8_t r[7];
    esp_err_t err = i2c_master_transmit_receive(s_dev, &reg, 1, r, sizeof r, I2C_TIMEOUT_MS);
    if (err != ESP_OK) {
        return err;
    }
    if (r[0] & OS_FLAG) {
        return ESP_ERR_INVALID_STATE;  // lost power: never set, or battery out
    }
    int sec = from_bcd(r[0] & 0x7f);
    int min = from_bcd(r[1] & 0x7f);
    int hour = from_bcd(r[2] & 0x3f);  // 24-hour mode (the default)
    int mday = from_bcd(r[3] & 0x3f);
    int mon = from_bcd(r[5] & 0x1f);
    int year = 2000 + from_bcd(r[6]);
    if (mon < 1 || mon > 12 || mday < 1 || mday > 31 || hour > 23 || min > 59 || sec > 59) {
        return ESP_ERR_INVALID_RESPONSE;
    }
    *utc = (time_t)days_from_civil(year, mon, mday) * 86400 + hour * 3600 + min * 60 + sec;
    return ESP_OK;
}

esp_err_t rtc_write(time_t utc)
{
    if (!s_dev) {
        return ESP_ERR_INVALID_STATE;
    }
    struct tm tm;
    gmtime_r(&utc, &tm);
    if (tm.tm_year < 100 || tm.tm_year > 199) {
        return ESP_ERR_INVALID_ARG;  // chip only stores 2000-2099
    }
    uint8_t w[8] = {
        REG_SECONDS,
        to_bcd(tm.tm_sec),  // writing seconds also clears the OS flag
        to_bcd(tm.tm_min),
        to_bcd(tm.tm_hour),
        to_bcd(tm.tm_mday),
        (uint8_t)tm.tm_wday,
        to_bcd(tm.tm_mon + 1),
        to_bcd(tm.tm_year - 100),
    };
    return i2c_master_transmit(s_dev, w, sizeof w, I2C_TIMEOUT_MS);
}
