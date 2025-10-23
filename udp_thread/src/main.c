#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <nrf_modem.h> 
#include <nrf_modem_at.h> 
#include <modem/nrf_modem_lib.h>
#include <modem/lte_lc.h>
#include <zephyr/net/socket.h>
#include <zephyr/net/net_ip.h>
#include <zephyr/drivers/gpio.h>
#include <stdlib.h>
#include <errno.h>

#define PIN_LED_0       23
#define PIN_LED_1       24
#define PIN_LED_2       25

LOG_MODULE_REGISTER(main_udp, CONFIG_LOG_DEFAULT_LEVEL);

static K_SEM_DEFINE(udpsem_start, 0, 1);

static const struct device *leds = DEVICE_DT_GET(DT_NODELABEL(gpio0));
static int socknum = NULL;
static uint8_t rxpktbuf[64];
static bool isconnected = false;

int main(void)
{
    nrf_modem_lib_init();

    LOG_INF("=====UDP EXAMPLE=====");
    
    struct sockaddr_in sa;
    int ret;
    uint8_t init_buf[1];
    init_buf[0] = 0;

    gpio_pin_configure(leds, PIN_LED_0, GPIO_OUTPUT_HIGH);
    gpio_pin_configure(leds, PIN_LED_1, GPIO_OUTPUT_HIGH);
    gpio_pin_configure(leds, PIN_LED_2, GPIO_OUTPUT_HIGH);

    lte_lc_connect();

    socknum = zsock_socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);

    sa.sin_family = AF_INET;
    sa.sin_port = htons(40000); /* set your server port */
    zsock_inet_pton(AF_INET, "your server IP", &sa.sin_addr); /* set your server IP */
    
	ret = zsock_connect(socknum, (struct sockaddr *)&sa, sizeof(struct sockaddr_in));
	if(ret < 0) {
		LOG_ERR("connect failed:%d", ret);
	}
    LOG_INF("udp connected...");
    isconnected = true;

    /* send one UDP packet to give the IP address and port to server */
    zsock_send(socknum, init_buf, sizeof(init_buf), 0);

    k_sem_give(&udpsem_start);

    k_msleep(60000); /* 60 seconds */

    zsock_close(socknum);
    lte_lc_power_off();
    socknum = NULL;
    isconnected = false;

    LOG_INF("udp disconnected...");
    LOG_INF("main close...");

    return 0;
}

static void udp_thread(void)
{
    int ret;
    struct zsock_pollfd fds[1]; 

    fds[0].fd = socknum;
    fds[0].events = ZSOCK_POLLIN;

    k_sem_take(&udpsem_start, K_FOREVER);
    LOG_INF("udp poll start...");

    while(socknum != NULL && isconnected == true) {
        ret = zsock_poll(fds, 1, 1000);
        if (ret < 0) {
            LOG_ERR("poll() failed: (%d)", -errno);
            ret = -EIO;
            continue;
	    }
        if (ret == 0) {
            continue;   /* poll() timeout */
        }
        if ((fds[0].revents & ZSOCK_POLLHUP) == ZSOCK_POLLHUP) {
            LOG_ERR("POLLHUP");
            break;
	    }
        if ((fds[0].revents & ZSOCK_POLLNVAL) == ZSOCK_POLLNVAL) {
            LOG_ERR("POLLNVAL");
            break;
        }
        if ((fds[0].revents & ZSOCK_POLLERR) == ZSOCK_POLLERR) {
            LOG_WRN("POLLERR");
            break;
        }
        if ((fds[0].revents & ZSOCK_POLLIN) == ZSOCK_POLLIN) {
            ret = zsock_recv(fds[0].fd, (void *)rxpktbuf, sizeof(rxpktbuf), 0);
            if (ret <= 0) {
                LOG_ERR("recv() failed: (%d)", -errno);
            }
            else if(ret >= 0) {
                LOG_INF("recv()=%d", ret);
                gpio_pin_set_raw(leds, PIN_LED_0, (rxpktbuf[0] == 1) ? 0 : 1);
                gpio_pin_set_raw(leds, PIN_LED_1, (rxpktbuf[1] == 1) ? 0 : 1);
                gpio_pin_set_raw(leds, PIN_LED_2, (rxpktbuf[2] == 1) ? 0 : 1);
            }
        } 
    }
    LOG_INF("udp thread close...");
    return;
}

K_THREAD_DEFINE(udp_thread_id, 2048, udp_thread, NULL, NULL, NULL, 1, 0, 0);
