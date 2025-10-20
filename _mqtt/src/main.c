#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <nrf_modem.h> 
#include <nrf_modem_at.h> 
#include <modem/nrf_modem_lib.h>
#include <modem/lte_lc.h>
#include <zephyr/net/socket.h>
#include <zephyr/net/net_ip.h>
#include <zephyr/net/mqtt.h>

LOG_MODULE_REGISTER(main_mqtt, CONFIG_LOG_DEFAULT_LEVEL);

#define CLIENT_ID     "cocolinx-mqtt-demo"
#define SUB_TOPIC     "cocolinx/examples"
#define PUB_TOPIC     "cocolinx/examples"
#define KEEPALIVE_SEC  60

static K_SEM_DEFINE(mqtt_conn_sem, 0, 1);

static struct sockaddr_storage broker;
static struct mqtt_client client;
static uint8_t rx_buf[64];
static uint8_t tx_buf[64];

bool mqtt_connected = false;

static void mqtt_evt_handler(struct mqtt_client *const client, const struct mqtt_evt *evt)
{
    switch (evt->type) 
	{
		case MQTT_EVT_CONNACK:
			LOG_INF("MQTT_EVT_CONNACK");
            mqtt_connected = true;
			k_sem_give(&mqtt_conn_sem);
			break;
		case MQTT_EVT_DISCONNECT:
            mqtt_connected = false;
			LOG_INF("MQTT_EVT_DISCONNECT");
			break;
		case MQTT_EVT_PUBLISH:
			LOG_INF("MQTT_EVT_PUBLISH");
			mqtt_read_publish_payload(client, rx_buf, evt->param.publish.message.payload.len);
            LOG_INF("mqtt recv: %.*s", evt->param.publish.message.payload.len, rx_buf);
			break;
        case MQTT_EVT_PUBACK:   /* Acknowledgment for published message with QoS 1. */           break;
        case MQTT_EVT_PUBREC:   /* Reception confirmation for published message with QoS 2. */   break;
        case MQTT_EVT_PUBREL:   /* Release of published message with QoS 2. */                   break;
        case MQTT_EVT_PUBCOMP:  /* Confirmation to a publish release message with QoS 2. */      break;
        case MQTT_EVT_SUBACK:   /* Acknowledgment to a subscribe request. */                     break;
        case MQTT_EVT_UNSUBACK: /* Acknowledgment to a unsubscribe request. */                   break;
        case MQTT_EVT_PINGRESP: /* Ping Response from server. */                                 break;
		default:
			LOG_INF("default");
			break;
	}	
}


int main(void)
{
    nrf_modem_lib_init();

    LOG_INF("=====MQTT EXAMPLE=====");

    struct sockaddr_in sa;
    int ret;
    uint16_t msg_id = 1;

    struct zsock_addrinfo *res = NULL;  
    struct zsock_addrinfo hints = {
        .ai_family   = AF_INET,
        .ai_socktype = SOCK_STREAM
    };

    struct mqtt_topic topic;
    struct mqtt_subscription_list sub_list;
    struct mqtt_publish_param pub_param;

    lte_lc_connect();

    sa.sin_family = AF_INET;
    sa.sin_port = htons(1883);

    zsock_getaddrinfo("test.mosquitto.org", NULL, &hints, &res); // get ip address
    sa.sin_addr = ((struct sockaddr_in *)res->ai_addr)->sin_addr;
    zsock_freeaddrinfo(res);

    // client init
    memcpy(&broker, &sa, sizeof(sa));

    mqtt_client_init(&client);
    client.broker           = (struct sockaddr *)&broker;
    client.evt_cb           = mqtt_evt_handler;
    client.client_id.utf8   = (uint8_t *)CLIENT_ID; // have to be unique
    client.client_id.size   = strlen(CLIENT_ID);
    client.protocol_version = MQTT_VERSION_3_1_1;
    client.transport.type   = MQTT_TRANSPORT_NON_SECURE;
    client.clean_session    = true;
    client.keepalive        = KEEPALIVE_SEC;
    client.password         = NULL;
	client.user_name        = NULL;
    client.rx_buf           = rx_buf; 
    client.tx_buf           = tx_buf; 
    client.rx_buf_size      = sizeof(rx_buf);
    client.tx_buf_size      = sizeof(tx_buf);

    // mqtt connection
    LOG_INF("mqtt connecting...");
    ret = mqtt_connect(&client);
    if (ret) { LOG_ERR("mqtt_connect rc=%d", ret); return 0; }

    struct zsock_pollfd pfd = {
        .fd = client.transport.tcp.sock,
        .events = ZSOCK_POLLIN
    };

    while (true) {
        ret = zsock_poll(&pfd, 1, 200);
        if (ret > 0 && (pfd.revents & ZSOCK_POLLIN)) {
            (void)mqtt_input(&client); 
        }
        (void)mqtt_live(&client); 
        if (k_sem_take(&mqtt_conn_sem, K_NO_WAIT) == 0) {
            break; 
        }
    }

    // mqtt subscribe
    topic.topic.utf8 = SUB_TOPIC;
    topic.topic.size = strlen(topic.topic.utf8);
    topic.qos = MQTT_QOS_0_AT_MOST_ONCE; 
    sub_list.list = &topic;
    sub_list.list_count = 1;
    sub_list.message_id = msg_id;

    ret = mqtt_subscribe(&client, &sub_list);

    // mqtt publish
    const char *msg = "hello cocolinx";

    pub_param.message.topic.topic.utf8 = (uint8_t *)PUB_TOPIC;
    pub_param.message.topic.topic.size = strlen(pub_param.message.topic.topic.utf8);
    pub_param.message.topic.qos        = MQTT_QOS_0_AT_MOST_ONCE;
    pub_param.message.payload.data     = (uint8_t *)msg;
    pub_param.message.payload.len      = strlen(pub_param.message.payload.data);
    pub_param.retain_flag              = 0;
    pub_param.dup_flag                 = 0;
    pub_param.message_id               = msg_id;

    ret = mqtt_publish(&client, &pub_param);

    while(mqtt_connected) {
        ret = zsock_poll(&pfd, 1, 200);
        if (ret != 0) {
            if (pfd.revents & ZSOCK_POLLIN) {
                /* MQTT data received */
                ret = mqtt_input(&client);
                if (ret != 0) {
                    LOG_ERR("MQTT Input failed [%d]", ret);
                    break;
                }
                /* Socket error */
                if (pfd.revents & (ZSOCK_POLLHUP | ZSOCK_POLLERR)) {
                    LOG_ERR("MQTT socket closed / error");
                    break;
                }
            }
        } 
        else {
            /* Socket poll timed out, time to call mqtt_live() */
            ret = mqtt_live(&client);
            if (ret != 0 && ret != -EAGAIN) {
                LOG_ERR("MQTT Live failed [%d]", ret);
            }
	    }
    }
    
    mqtt_connected = false;
    LOG_INF("mqtt disposed");

    return 0;
}
