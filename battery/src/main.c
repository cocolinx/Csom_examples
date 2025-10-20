#include <zephyr/kernel.h>
#include <zephyr/devicetree.h>
#include <zephyr/logging/log.h>
#include <modem/modem_info.h>
#include <modem/nrf_modem_lib.h>

LOG_MODULE_REGISTER(main);

int main(void)
{
    nrf_modem_lib_init();
    
    LOG_INF("=====Battery Voltage Sample=====");
    
    modem_info_init();
    
    /* Get battery voltage */
    int voltage = 0;
    modem_info_get_batt_voltage(&voltage);
    LOG_INF("Battery Voltage: %d mV", voltage);

    return 0;
}