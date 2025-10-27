#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/drivers/gpio.h>
#include <nrf_modem.h> 
#include <nrf_modem_at.h> 
#include <modem/nrf_modem_lib.h>

#define STEP_DELAY_MS  5 

LOG_MODULE_REGISTER(main_pins, CONFIG_LOG_DEFAULT_LEVEL);

static const uint8_t PINS[] = {
    8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 28, 29, 30, 31
};

static const struct device *port = DEVICE_DT_GET(DT_NODELABEL(gpio0));

static void sweep_gpio0(const uint8_t *pins, size_t cnt)
{
    if (!device_is_ready(port)) {
        LOG_ERR("gpio0 not ready");
        return;
    }

    for (size_t i = 0; i < cnt; i++) {
        uint8_t pin = pins[i];

        int ret = gpio_pin_configure(port, pin, GPIO_OUTPUT | GPIO_INPUT);
        if (ret) {
            if (ret == -EBUSY) {
                LOG_WRN("P0.%02u busy(-EBUSY)", pin);
            } else {
                LOG_ERR("P0.%02u configure failed (%d)", pin, ret);
            }
            continue;
        }

        /* set low */
        ret = gpio_pin_set_raw(port, pin, 0);
        if (ret) {
            LOG_ERR("P0.%02u set LOW failed (%d)", pin, ret);
        }
        k_msleep(STEP_DELAY_MS);

        int v_on = gpio_pin_get_raw(port, pin);
        if (v_on < 0) {
            LOG_ERR("P0.%02u get after LOW failed (%d)", pin, v_on);
        }

        LOG_INF("P0.%02u: set=LOW   read=%d, %s", pin, v_on, (v_on==0)? "OKAY" : "NOT OKAY");

        /* set high */
        ret = gpio_pin_set_raw(port, pin, 1);
        if (ret) {
            LOG_ERR("P0.%02u set HIGH failed (%d)", pin, ret);
        }
        k_msleep(STEP_DELAY_MS);

        int v_off = gpio_pin_get_raw(port, pin);
        if (v_off < 0) {
            LOG_ERR("P0.%02u get after HIGH failed (%d)", pin, v_off);
        }
        LOG_INF("P0.%02u: set=HIGH  read=%d, %s", pin, v_off, (v_off==1)? "OKAY" : "NOT OKAY");
    }       
}

int main(void)
{
    nrf_modem_lib_init();
    LOG_INF("=====TEST_PINS EXAMPLES=====");
    
    LOG_INF("=== gpio0 pin sweep start ===");
    sweep_gpio0(PINS, ARRAY_SIZE(PINS));
    LOG_INF("=== gpio0 pin sweep done ===");
    return 0;
}
