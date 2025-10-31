#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <modem/nrf_modem_lib.h>
#include <zephyr/drivers/gpio.h>

#define DEBOUNCE_MS 20
#define PIN_BUTTON1 18

LOG_MODULE_REGISTER(main_button, CONFIG_LOG_DEFAULT_LEVEL);
static const struct device *btn = DEVICE_DT_GET(DT_NODELABEL(gpio0));

static void btn_debounce_work_handler(struct k_work *work);
K_WORK_DELAYABLE_DEFINE(btn_debounce_work, btn_debounce_work_handler);
static struct gpio_callback btn_cb;

static void btn_isr(const struct device *port, struct gpio_callback *cb, uint32_t pins)
{
    (void)k_work_reschedule(&btn_debounce_work, K_MSEC(DEBOUNCE_MS));
}

static void btn_debounce_work_handler(struct k_work *work)
{
    int v = gpio_pin_get_raw(btn, PIN_BUTTON1); 
    bool pressed = (v == 0) ? true : false;
    if(pressed) LOG_INF("button pressed");
    else LOG_INF("button released");
}

int main(void)
{
    nrf_modem_lib_init();
    
    LOG_INF("=====BUTTON EXAMPLE=====");
    
    gpio_pin_configure(btn, PIN_BUTTON1, GPIO_INPUT | GPIO_PULL_UP);
    gpio_pin_interrupt_configure(btn, PIN_BUTTON1, GPIO_INT_DISABLE);

    gpio_init_callback(&btn_cb, btn_isr, BIT(PIN_BUTTON1));
    gpio_add_callback(btn, &btn_cb);
    gpio_pin_interrupt_configure(btn, PIN_BUTTON1, GPIO_INT_EDGE_BOTH); // rising, falling

    LOG_INF("press button to test!");

    return 0;
}
