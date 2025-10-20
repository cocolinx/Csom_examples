#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <nrf_modem.h> 
#include <nrf_modem_at.h> 
#include <modem/nrf_modem_lib.h>

LOG_MODULE_REGISTER(main_helloworld, CONFIG_LOG_DEFAULT_LEVEL);

int main(void)
{
    nrf_modem_lib_init();
    
    LOG_INF("Hello World");

    return 0;
}
