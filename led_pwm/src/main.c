#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/drivers/pwm.h>
#include <nrf_modem.h> 
#include <nrf_modem_at.h> 
#include <nrfx_pwm.h>
#include <modem/nrf_modem_lib.h>

LOG_MODULE_REGISTER(main_pwm, CONFIG_LOG_DEFAULT_LEVEL);

/*pin 0.28*/
static const struct device *led_pwm = DEVICE_DT_GET(DT_NODELABEL(pwm0));

int main(void)
{
    nrf_modem_lib_init();
    LOG_INF("=====PWM EXAMPLE=====");

    /*you need to set width and period value*/
    uint32_t width =   10000;
    uint32_t period = 100000;
    pwm_set(led_pwm, 0, period, width, PWM_POLARITY_NORMAL);

    while (true)
    {        
        k_msleep(1000);
        width += 10000;
        if(width >= period) width = 10000;
        pwm_set(led_pwm, 0, period, width, PWM_POLARITY_NORMAL);
    }

    return 0;
}
