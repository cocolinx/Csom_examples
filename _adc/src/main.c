#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <nrf_modem.h> 
#include <nrf_modem_at.h> 
#include <modem/nrf_modem_lib.h>
#include <zephyr/drivers/adc.h>
#include <hal/nrf_saadc.h>

LOG_MODULE_REGISTER(main_adc, CONFIG_LOG_DEFAULT_LEVEL);

static const struct device *adc_dev = DEVICE_DT_GET(DT_NODELABEL(adc));
static struct adc_channel_cfg channel_cfg;
static struct adc_sequence adc_sequence;

static uint16_t adc_buffer;

int main(void)
{
    nrf_modem_lib_init();
    LOG_INF("=====ADC EXAMPLE=====");

    int mv;

    /* adc init */
    channel_cfg.channel_id = 1;
    channel_cfg.gain = ADC_GAIN_1_5; /* 3.0v */
    channel_cfg.reference = ADC_REF_INTERNAL;
    channel_cfg.acquisition_time = ADC_ACQ_TIME(ADC_ACQ_TIME_MICROSECONDS, 40);
    channel_cfg.differential = false;
    channel_cfg.input_positive = NRF_SAADC_INPUT_AIN2; 
    channel_cfg.input_negative = NRF_SAADC_INPUT_DISABLED;

    adc_sequence.options = NULL;
    adc_sequence.channels = BIT(1);
    adc_sequence.buffer = &adc_buffer;
    adc_sequence.buffer_size = 2;
    adc_sequence.resolution = 14;
    adc_sequence.oversampling = 4;
    adc_sequence.calibrate = true;

    adc_channel_setup(adc_dev, &channel_cfg);

    while(true) {
        adc_read(adc_dev, &adc_sequence);
        mv = adc_buffer;
        mv = (mv * 3000) / 16383; /* mv: 0~3000 */
        LOG_INF("adc value: %d", mv);
        k_msleep(1000);
    }
    return 0;
}
