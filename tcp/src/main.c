#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <nrf_modem.h> 
#include <nrf_modem_at.h> 
#include <modem/nrf_modem_lib.h>
#include <modem/lte_lc.h>
#include <zephyr/net/socket.h>
#include <zephyr/net/net_ip.h>

LOG_MODULE_REGISTER(main_tcp, CONFIG_LOG_DEFAULT_LEVEL);

static K_SEM_DEFINE(tcpsem_start, 0, 1);

static int socknum;
static uint8_t rxpktbuf[64];
static bool isconnected = false;

int main(void)
{
    nrf_modem_lib_init();

    LOG_INF("=====TCP EXAMPLE=====");
    
    struct sockaddr_in sa;
    int ret;

    lte_lc_connect();

    socknum = zsock_socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

    sa.sin_family = AF_INET;
    sa.sin_port = htons(7777);
    zsock_inet_pton(AF_INET, "43.200.166.133", &sa.sin_addr); /* echo server IP address */
    
	ret = zsock_connect(socknum, (struct sockaddr *)&sa, sizeof(struct sockaddr_in));
	if(ret < 0) {
		LOG_ERR("connect failed:%d", ret);
	}
    LOG_INF("tcp connected...");
    isconnected = true;

    k_sem_give(&tcpsem_start);

    int txcnt = 5;
    while(txcnt--){
        uint8_t txbuf[] = "this is test for tcp";
        LOG_INF("send.....%d", txcnt);
        ret = zsock_send(socknum, txbuf, sizeof(txbuf), 0);
        k_msleep(5000);
    }

    isconnected = false;
    LOG_INF("tcp close...");
    LOG_INF("main close...");

    return 0;
}

static void tcp_thread(void)
{
    int ret;
    struct zsock_pollfd fds[1]; 

    fds[0].fd = socknum;
    fds[0].events = ZSOCK_POLLIN;

    k_sem_take(&tcpsem_start, K_FOREVER);
    LOG_INF("udp poll start...");

    while(isconnected) {
        ret = zsock_poll(fds, 1, 1000);
        if (ret < 0) {
            LOG_ERR("poll() failed: (%d)", -errno);
            ret = -EIO;
            break;
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
                LOG_INF("recv(%d): %.*s", ret, ret, rxpktbuf);
            }
        } 
    }

    zsock_close(socknum);
    lte_lc_power_off();
    LOG_INF("tcp thread close...");
    return;
}

K_THREAD_DEFINE(tcp_thread_id, 2048, tcp_thread, NULL, NULL, NULL, 1, 0, 0);
