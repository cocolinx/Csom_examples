#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <nrf_modem.h> 
#include <nrf_modem_at.h> 
#include <modem/nrf_modem_lib.h>
#include <modem/lte_lc.h>
#include <zephyr/net/socket.h>
#include <zephyr/net/net_ip.h>

LOG_MODULE_REGISTER(main_udp, CONFIG_LOG_DEFAULT_LEVEL);

int main(void)
{
    nrf_modem_lib_init();

    LOG_INF("=====UDP EXAMPLE=====");
    
    struct sockaddr_in sa;
    uint8_t rxbuf[32];
    int ret;
    int socknum;

    lte_lc_connect();

    socknum = zsock_socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);

    sa.sin_family = AF_INET;
    sa.sin_port = htons(7777);
    zsock_inet_pton(AF_INET, "43.200.166.133", &sa.sin_addr); /* echo server IP address */
     
	ret = zsock_connect(socknum, (struct sockaddr *)&sa, sizeof(struct sockaddr_in));
	if(ret < 0) {
		LOG_ERR("connect failed:%d", ret);
	}

    memset(rxbuf, 0, sizeof(rxbuf));

    int txcnt = 5;
    while(txcnt--){
        uint8_t txbuf[] = "this is test for udp";
        LOG_INF("send.....%d", txcnt);
        ret = zsock_send(socknum, txbuf, sizeof(txbuf), 0);

        ret = zsock_recv(socknum, rxbuf, sizeof(rxbuf), 0);
        if(ret > 0) LOG_INF("recv %d bytes : %.*s", ret, ret, rxbuf);

        k_msleep(5000);
    }

    zsock_close(socknum);
    lte_lc_power_off();
    LOG_INF("udp close...");
    LOG_INF("main close...");

    return 0;
}
