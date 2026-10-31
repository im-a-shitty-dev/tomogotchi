#include "power.h"

#include "bsp/esp-bsp.h"
#include "driver/gpio.h"
#include "driver/i2c_master.h"
#include "esp_log.h"
#include "esp_pm.h"
#include "esp_sleep.h"
#include "esp_timer.h"

static const char *TAG = "power";

#if CONFIG_IDF_TARGET_ESP32S3
#define BOOT_GPIO GPIO_NUM_0         // BOOT button, active low
#else
#define BOOT_GPIO GPIO_NUM_9         // BOOT button, active low
#endif
#define TAP_GPIO BSP_LCD_TOUCH_INT   // touch chip interrupt, active low
#define BUTTON_DEBOUNCE_MS 250
#define I2C_TIMEOUT_MS 50

// The S3 board package doesn't name the power chip's address.
#ifndef BSP_PMU_I2C_ADDRESS
#define BSP_PMU_I2C_ADDRESS 0x34
#endif

// AXP2101 registers (see XPowersLib's AXP2101Constants.h).
#define AXP_ADDR BSP_PMU_I2C_ADDRESS
#define AXP_STATUS1 0x00   // bit 5: VBUS good
#define AXP_INTEN2 0x41
#define AXP_INTSTS2 0x49   // write 1 to clear
#define AXP_PKEY_SHORT (1u << 3)
#define AXP_BAT_PERCENT 0xA4
#define AXP_VBUS_GOOD (1u << 5)
#define AXP_BAT_PRESENT (1u << 3)

static i2c_master_dev_handle_t s_axp;
static TaskHandle_t s_waker;
static volatile bool s_boot_pressed;
static volatile bool s_tapped;
static bool s_dozing;
static int64_t s_last_boot_us;

static esp_err_t axp_read(uint8_t reg, uint8_t *v)
{
    return i2c_master_transmit_receive(s_axp, &reg, 1, v, 1, I2C_TIMEOUT_MS);
}

static esp_err_t axp_write(uint8_t reg, uint8_t v)
{
    uint8_t w[2] = {reg, v};
    return i2c_master_transmit(s_axp, w, sizeof w, I2C_TIMEOUT_MS);
}

// One handler for both lines. While dozing they're level-triggered (light
// sleep can only wake on a level), so each one switches itself off until
// power_set_dozing(false) puts it back.
static void IRAM_ATTR on_wake_line(void *arg)
{
    gpio_num_t pin = (gpio_num_t)(intptr_t)arg;
    if (s_dozing) {
        gpio_intr_disable(pin);
    }
    if (pin == BOOT_GPIO) {
        s_boot_pressed = true;
    } else {
        s_tapped = true;
    }
    BaseType_t woken = pdFALSE;
    if (s_waker) {
        vTaskNotifyGiveFromISR(s_waker, &woken);
    }
    portYIELD_FROM_ISR(woken);
}

static esp_err_t pmu_start(void)
{
    i2c_master_bus_handle_t bus = bsp_i2c_get_handle();
    if (bus == NULL) {
        return ESP_ERR_INVALID_STATE;
    }
    esp_err_t err = i2c_master_probe(bus, AXP_ADDR, I2C_TIMEOUT_MS);
    if (err != ESP_OK) {
        return err;
    }
    i2c_device_config_t cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = AXP_ADDR,
        .scl_speed_hz = 400000,
    };
    if ((err = i2c_master_bus_add_device(bus, &cfg, &s_axp)) != ESP_OK) {
        return err;
    }
    // Latch PWR short presses. Its IRQ pin isn't wired to us, so we poll.
    uint8_t en;
    if ((err = axp_read(AXP_INTEN2, &en)) != ESP_OK) {
        return err;
    }
    axp_write(AXP_INTEN2, en | AXP_PKEY_SHORT);
    return axp_write(AXP_INTSTS2, AXP_PKEY_SHORT);
}

esp_err_t power_start(TaskHandle_t waker)
{
    s_waker = waker;

    esp_err_t err = pmu_start();
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "no power chip (%s): no charger or PWR button", esp_err_to_name(err));
        s_axp = NULL;
    }

    const gpio_config_t io = {
        .pin_bit_mask = (1ull << BOOT_GPIO) | (1ull << TAP_GPIO),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    ESP_ERROR_CHECK(gpio_config(&io));
    err = gpio_install_isr_service(0);
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {  // already installed is fine
        return err;
    }
    gpio_isr_handler_add(BOOT_GPIO, on_wake_line, (void *)(intptr_t)BOOT_GPIO);
    gpio_isr_handler_add(TAP_GPIO, on_wake_line, (void *)(intptr_t)TAP_GPIO);
    ESP_ERROR_CHECK(esp_sleep_enable_gpio_wakeup());

    power_set_dozing(false);
    return ESP_OK;
}

power_status_t power_poll(void)
{
    power_status_t st = {.battery_pct = -1};

    if (s_boot_pressed) {
        s_boot_pressed = false;
        int64_t now = esp_timer_get_time();
        if (now - s_last_boot_us >= BUTTON_DEBOUNCE_MS * 1000) {
            st.button = true;
        }
        s_last_boot_us = now;
    }
    if (s_tapped) {
        s_tapped = false;
        st.tap = true;
    }

    if (s_axp) {
        uint8_t status, irq, pct;
        if (axp_read(AXP_STATUS1, &status) == ESP_OK) {
            st.charging = status & AXP_VBUS_GOOD;
            if ((status & AXP_BAT_PRESENT) && axp_read(AXP_BAT_PERCENT, &pct) == ESP_OK) {
                st.battery_pct = pct;
            }
        }
        if (axp_read(AXP_INTSTS2, &irq) == ESP_OK && (irq & AXP_PKEY_SHORT)) {
            axp_write(AXP_INTSTS2, AXP_PKEY_SHORT);
            st.button = true;
        }
    }
    return st;
}

static void configure_pm(bool light_sleep)
{
    const esp_pm_config_t pm = {
        .max_freq_mhz = CONFIG_ESP_DEFAULT_CPU_FREQ_MHZ,
        .min_freq_mhz = light_sleep ? 40 : CONFIG_ESP_DEFAULT_CPU_FREQ_MHZ,
        .light_sleep_enable = light_sleep,
    };
    esp_err_t err = esp_pm_configure(&pm);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "power management: %s", esp_err_to_name(err));
    }
}

// Wait (briefly) for a line to go high, so arming its wake-up doesn't fire
// straight away on the press that turned the screen off.
static void wait_released(gpio_num_t pin)
{
    for (int i = 0; i < 100 && gpio_get_level(pin) == 0; i++) {
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

void power_set_dozing(bool dozing)
{
    s_dozing = dozing;
    if (dozing) {
        wait_released(BOOT_GPIO);
        wait_released(TAP_GPIO);
        s_tapped = false;
        s_boot_pressed = false;
        // gpio_wakeup_enable also makes the pin interrupt level-triggered.
        gpio_wakeup_enable(BOOT_GPIO, GPIO_INTR_LOW_LEVEL);
        gpio_wakeup_enable(TAP_GPIO, GPIO_INTR_LOW_LEVEL);
        gpio_intr_enable(BOOT_GPIO);
        gpio_intr_enable(TAP_GPIO);
        configure_pm(true);
    } else {
        configure_pm(false);
        gpio_wakeup_disable(BOOT_GPIO);
        gpio_wakeup_disable(TAP_GPIO);
        // Awake, LVGL reads the touch chip itself; only BOOT needs an interrupt.
        gpio_intr_disable(TAP_GPIO);
        gpio_set_intr_type(BOOT_GPIO, GPIO_INTR_NEGEDGE);
        gpio_intr_enable(BOOT_GPIO);
    }
}
