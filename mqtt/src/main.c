#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <modem/nrf_modem_lib.h>
#include <modem/lte_lc.h>
#include <zephyr/net/socket.h>
#include <zephyr/net/net_ip.h>
#include <zephyr/net/mqtt.h>

#define CLIENT_ID     "cocolinx-mqtt-demo"
#define SUB_TOPIC     "cocolinx/examples"
#define PUB_TOPIC     "cocolinx/examples"
#define KEEPALIVE_SEC  60

LOG_MODULE_REGISTER(main_mqtt, CONFIG_LOG_DEFAULT_LEVEL);

static K_SEM_DEFINE(udpsem_start, 0, 1);

static K_SEM_DEFINE(mqtt_conn_sem, 0, 1);

static struct sockaddr_storage broker;
static struct mqtt_client client;
static struct zsock_pollfd pfd;
static struct mqtt_topic topic;
static struct mqtt_subscription_list sub_list;
static struct mqtt_publish_param pub_param;
static uint8_t rxmsg_payload[1024];
static uint8_t txmsg_payload[1024];

bool mqtt_connected = false;

static int handle_mqtt_publish_evt(struct mqtt_client *const c, const struct mqtt_evt *evt)
{	
	/* Send QoS acknowledgments */
	if (evt->param.publish.message.topic.qos == MQTT_QOS_1_AT_LEAST_ONCE) {
		const struct mqtt_puback_param ack = {
			.message_id = evt->param.publish.message_id
		};

		mqtt_publish_qos1_ack(&client, &ack);
	} else if (evt->param.publish.message.topic.qos == MQTT_QOS_2_EXACTLY_ONCE) {
		const struct mqtt_pubrec_param ack = {
			.message_id = evt->param.publish.message_id
		};

		mqtt_publish_qos2_receive(&client, &ack);
	}

	/* SLM MQTT client does not track the packet identifiers, so MQTT_QOS_2_EXACTLY_ONCE
	 * promise is not kept. This deviates from MQTT v3.1.1.
	 */

	/* topic size, payload len */
	int topic_size = evt->param.publish.message.topic.topic.size;
	int payload_size = evt->param.publish.message.payload.len;
	if(payload_size < 0) payload_size = 0;
	LOG_INF("topic size: %d, payload size: %d", topic_size, payload_size);

	/* get payload */
	int size_read = 0;	
	int bytes;	

	while(true) {
		bytes = mqtt_read_publish_payload_blocking(c, rxmsg_payload, sizeof(rxmsg_payload));		
		LOG_INF("mqtt recv: %.*s", bytes, rxmsg_payload);
		
		if(bytes > 0) 
		{			
			size_read += bytes;
			if(size_read >= payload_size) break;
		} else break;		
	}	
	
	return 0;
}


static void mqtt_evt_handler(struct mqtt_client *const client, const struct mqtt_evt *evt)
{
    int ret; 

    switch (evt->type) 
	{
		case MQTT_EVT_CONNACK:
			LOG_INF("MQTT_EVT_CONNACK");
            mqtt_connected = true;
			k_sem_give(&mqtt_conn_sem);
			break;
		case MQTT_EVT_DISCONNECT:
            LOG_INF("MQTT_EVT_DISCONNECT");
            mqtt_connected = false;
			break;
		case MQTT_EVT_PUBLISH:
			LOG_INF("MQTT_EVT_PUBLISH");
            ret = handle_mqtt_publish_evt(client, evt);
			break;
        case MQTT_EVT_PUBACK:
            if (evt->result == 0) {
                LOG_DBG("PUBACK packet id: %u", evt->param.puback.message_id);
            }
            break;

        case MQTT_EVT_PUBREC:
            if (evt->result != 0) {
                break;
            }
            LOG_DBG("PUBREC packet id: %u", evt->param.pubrec.message_id);
            {
                struct mqtt_pubrel_param param = {
                    .message_id = evt->param.pubrec.message_id
                };
                ret = mqtt_publish_qos2_release(&client, &param);
                if (ret) {
                    LOG_ERR("mqtt_publish_qos2_release: Fail! %d", ret);
                } else {
                    LOG_DBG("Release, id %u", evt->param.pubrec.message_id);
                }
            }
            break;

        case MQTT_EVT_PUBREL:
            if (evt->result != 0) {
                break;
            }
            LOG_DBG("PUBREL packet id %u", evt->param.pubrel.message_id);
            {
                struct mqtt_pubcomp_param param = {
                    .message_id = evt->param.pubrel.message_id
                };
                ret = mqtt_publish_qos2_complete(&client, &param);
                if (ret) {
                    LOG_ERR("mqtt_publish_qos2_complete Failed:%d", ret);
                } else {
                    LOG_DBG("Complete, id %u", evt->param.pubrel.message_id);
                }
            }
            break;

        case MQTT_EVT_PUBCOMP:
            if (evt->result == 0) {
                LOG_DBG("PUBCOMP packet id %u", evt->param.pubcomp.message_id);
            }
            break;

        case MQTT_EVT_SUBACK:
            if (evt->result == 0) {
                LOG_DBG("SUBACK packet id: %u", evt->param.suback.message_id);
            }
            break;

        case MQTT_EVT_UNSUBACK:
            if (evt->result == 0) {
                LOG_DBG("UNSUBACK packet id: %u", evt->param.unsuback.message_id);
            }
            break;

        case MQTT_EVT_PINGRESP:
            if (evt->result == 0) {
                LOG_DBG("PINGRESP packet");
            }
            break;
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
    int iter = 5;
    uint16_t msg_id = 1;

    struct zsock_addrinfo *res = NULL;  
    struct zsock_addrinfo hints = {
        .ai_family   = AF_INET,
        .ai_socktype = SOCK_STREAM
    };

    lte_lc_connect();

    sa.sin_family = AF_INET;
    sa.sin_port = htons(1883);

    zsock_getaddrinfo("test.mosquitto.org", NULL, &hints, &res); /* get ip address */
    sa.sin_addr = ((struct sockaddr_in *)res->ai_addr)->sin_addr;
    zsock_freeaddrinfo(res);

    /* client init */
    memcpy(&broker, &sa, sizeof(sa));

    mqtt_client_init(&client);
    client.broker           = (struct sockaddr *)&broker;
    client.evt_cb           = mqtt_evt_handler;
    client.client_id.utf8   = (uint8_t *)CLIENT_ID; /* have to be unique */
    client.client_id.size   = strlen(CLIENT_ID);
    client.protocol_version = MQTT_VERSION_3_1_1;
    client.transport.type   = MQTT_TRANSPORT_NON_SECURE;
    client.clean_session    = true;
    client.keepalive        = KEEPALIVE_SEC;
    client.password         = NULL;
	client.user_name        = NULL;
    client.rx_buf           = rxmsg_payload; 
    client.tx_buf           = txmsg_payload; 
    client.rx_buf_size      = sizeof(rxmsg_payload);
    client.tx_buf_size      = sizeof(txmsg_payload);

    /* mqtt connection */
    LOG_INF("mqtt connecting...");
    ret = mqtt_connect(&client);
    if (ret) { LOG_ERR("mqtt_connect rc=%d", ret); return 0; }

    pfd.fd = client.transport.tcp.sock;
    pfd.events = ZSOCK_POLLIN;

    ret = zsock_poll(&pfd, 1, 10000);
    if (ret > 0 && (pfd.revents & ZSOCK_POLLIN)) {
        (void)mqtt_input(&client); 
    }
    if(ret == 0) {
        LOG_ERR("mqtt connection time out");
        return 0;
    }

    /* mqtt subscribe */
    topic.topic.utf8 = SUB_TOPIC;
    topic.topic.size = strlen(topic.topic.utf8);
    topic.qos = MQTT_QOS_1_AT_LEAST_ONCE; 
    sub_list.list = &topic;
    sub_list.list_count = 1;
    sub_list.message_id = msg_id;

    ret = mqtt_subscribe(&client, &sub_list);

    /* mqtt publish */
    const char *msg = "hello cocolinx";

    pub_param.message.topic.topic.utf8 = (uint8_t *)PUB_TOPIC;
    pub_param.message.topic.topic.size = strlen(pub_param.message.topic.topic.utf8);
    pub_param.message.topic.qos        = MQTT_QOS_1_AT_LEAST_ONCE;
    pub_param.message.payload.data     = (uint8_t *)msg;
    pub_param.message.payload.len      = strlen(pub_param.message.payload.data);
    pub_param.retain_flag              = 0;
    pub_param.dup_flag                 = 0;
    pub_param.message_id               = msg_id;


    while(iter--) {
        LOG_INF("mqtt publish...%d", iter);
        ret = mqtt_publish(&client, &pub_param);
        k_msleep(2000);
    }

    mqtt_disconnect(&client, NULL);
    LOG_INF("mqtt disconnected...");

    return 0;
}

static void mqtt_thread(void)
{
    int ret;

    k_sem_take(&mqtt_conn_sem, K_FOREVER);
    LOG_INF("mqtt thread start...");

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

    LOG_INF("mqtt thread close...");
    return;
}

K_THREAD_DEFINE(mqtt_thread_id, 2048, mqtt_thread, NULL, NULL, NULL, 1, 0, 0);