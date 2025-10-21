#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <nrf_modem.h> 
#include <nrf_modem_at.h> 
#include <nrf_modem_gnss.h>
#include <modem/nrf_modem_lib.h>
#include <modem/lte_lc.h>

LOG_MODULE_REGISTER(main_gnss, CONFIG_LOG_DEFAULT_LEVEL);

static struct nrf_modem_gnss_nmea_data_frame gnss_nmea_data;
static struct nrf_modem_gnss_pvt_data_frame agnss_data;
static bool nrf_modem_gnss_fix;

static void gnss_event_handler(int event)
{
    int retval;

	switch (event) {
	case NRF_MODEM_GNSS_EVT_PVT:
        retval = nrf_modem_gnss_read(&agnss_data, sizeof(agnss_data), NRF_MODEM_GNSS_DATA_PVT);
		if (retval) {
			LOG_ERR("nrf_modem_gnss_read failed, err %d", retval);
		}
        // LOG_INF("EVT_PVT flags: %d," agnss_data.flags);
        for (int i = 0; i < NRF_MODEM_GNSS_MAX_SATELLITES; ++i) {
            /* SV number 0 indicates no satellite */
            if (agnss_data.sv[i].sv) {
                LOG_INF("SV:%3d sig: %d c/n0:%4d el:%3d az:%3d in-fix: %d unhealthy: %d",
                agnss_data.sv[i].sv, agnss_data.sv[i].signal, agnss_data.sv[i].cn0,
                agnss_data.sv[i].elevation, agnss_data.sv[i].azimuth,
                (agnss_data.sv[i].flags & NRF_MODEM_GNSS_SV_FLAG_USED_IN_FIX) ? 1 : 0,
                (agnss_data.sv[i].flags & NRF_MODEM_GNSS_SV_FLAG_UNHEALTHY) ? 1 : 0);
            }
        }
        LOG_INF("latitude:   %f", agnss_data.latitude);
        LOG_INF("longitude:  %f", agnss_data.longitude);
        LOG_INF("-------------------------");
        if (agnss_data.flags & NRF_MODEM_GNSS_PVT_FLAG_FIX_VALID) {
			nrf_modem_gnss_fix = true;
		} else {
			nrf_modem_gnss_fix = false;
		}
		break;
	case NRF_MODEM_GNSS_EVT_FIX:
		LOG_DBG("NRF_MODEM_GNSS_EVT_FIX");
		break;
	case NRF_MODEM_GNSS_EVT_NMEA:
		if (nrf_modem_gnss_fix == true) {
			retval = nrf_modem_gnss_read(&gnss_nmea_data, sizeof(struct nrf_modem_gnss_nmea_data_frame), NRF_MODEM_GNSS_DATA_NMEA);
			if (retval) {
				LOG_ERR("nrf_modem_gnss_read failed, err: %d", retval);
				return;
			}
			LOG_INF("GNSS NMEA data : %*.s", sizeof(gnss_nmea_data.nmea_str), gnss_nmea_data.nmea_str);
			break;
		}
        int ret = nrf_modem_gnss_read(&gnss_nmea_data, sizeof(gnss_nmea_data),
                                  NRF_MODEM_GNSS_DATA_NMEA);
        if (ret == 0) {
            int len = strnlen(gnss_nmea_data.nmea_str, sizeof(gnss_nmea_data.nmea_str));
            LOG_INF("NMEA: %.*s", len, gnss_nmea_data.nmea_str);
        } 
        else {
            LOG_ERR("nrf_modem_gnss_read(NMEA) err=%d", ret);
        }
        break;
	case NRF_MODEM_GNSS_EVT_AGNSS_REQ:
		LOG_INF("Requesting A-GNSS Data");
		break;
	case NRF_MODEM_GNSS_EVT_BLOCKED:
		LOG_DBG("NRF_MODEM_GNSS_EVT_BLOCKED");
		break;
	case NRF_MODEM_GNSS_EVT_UNBLOCKED:
		LOG_DBG("NRF_MODEM_GNSS_EVT_UNBLOCKED");
		break;
	case NRF_MODEM_GNSS_EVT_PERIODIC_WAKEUP:
		LOG_DBG("NRF_MODEM_GNSS_EVT_PERIODIC_WAKEUP");
		break;
	case NRF_MODEM_GNSS_EVT_SLEEP_AFTER_TIMEOUT:
		LOG_DBG("NRF_MODEM_GNSS_EVT_SLEEP_AFTER_TIMEOUT");
		break;
	case NRF_MODEM_GNSS_EVT_SLEEP_AFTER_FIX:
		LOG_DBG("NRF_MODEM_GNSS_EVT_SLEEP_AFTER_FIX");
		break;
	default:
		LOG_WRN("Unknown GNSS event %d", event);
		break;
	}
}

int main(void)
{
    nrf_modem_lib_init();
    LOG_INF("=====GNSS EXAMPLE=====");

    int err;

    // lte_lc_connect();
    lte_lc_func_mode_set(LTE_LC_FUNC_MODE_ACTIVATE_GNSS);

    err = nrf_modem_gnss_event_handler_set(gnss_event_handler);
	if (err) {
		LOG_ERR("Failed to set GNSS event handler %d", err);
		return 0;
	}

	uint16_t nmea_mask = NRF_MODEM_GNSS_NMEA_GGA_MASK| NRF_MODEM_GNSS_NMEA_GSV_MASK | NRF_MODEM_GNSS_NMEA_GSA_MASK;
	err = nrf_modem_gnss_nmea_mask_set(nmea_mask);
	if (err) {
		LOG_ERR("Failed to set GNSS NMEA mask %d", err);
		return 0;
	}

	/* This use case flag should always be set. */
	uint8_t use_case = NRF_MODEM_GNSS_USE_CASE_MULTIPLE_HOT_START;
	err = nrf_modem_gnss_use_case_set(use_case);
	if (err) {
		LOG_WRN("Failed to set GNSS use case %d", err);
	}

	err = nrf_modem_gnss_fix_retry_set(0);
	if (err) {
		LOG_ERR("Failed to set GNSS fix retry %d", err);
		return 0;
	}

	err = nrf_modem_gnss_fix_interval_set(1);
	if (err) {
		LOG_ERR("Failed to set GNSS fix interval %d", err);
		return 0;
	}

	LOG_INF("GNSS initialized");

	err = nrf_modem_gnss_start();
	if (err) {
		LOG_ERR("Failed to start GNSS, err: %d", err);
		return 0;
	}

    return 0;
}
