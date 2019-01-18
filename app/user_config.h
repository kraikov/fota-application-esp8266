#ifndef __USER_CONFIG_H__
#define __USER_CONFIG_H__


#define MQTT_SSL_ENABLE

#define MQTT_HOST               "m20.cloudmqtt.com"
#define MQTT_PORT               11653
#define MQTT_BUF_SIZE           1024
#define MQTT_KEEPALIVE          20
#define MQTT_CLEAN_SESSION      1

#define MQTT_CLIENT_ID          "ESP"
#define MQTT_USER               "kraykov"
#define MQTT_PASS               "qwerty123"

#define MQTT_RECONNECT_TIMEOUT  5

#define UpdateTopic        "esp/update"
#define RevertTopic        "esp/revert"

#define DEFAULT_SECURITY        0
#define QUEUE_BUFFER_SIZE       2048

#define PROTOCOL_NAMEv31

#define LED_GPIO        2

#define LED_OFF_VALUE   1

#define LED_ON_VALUE    0

#define LED_ON() easygpio_outputSet(LED_GPIO, LED_ON_VALUE)

#define LED_OFF() easygpio_outputSet(LED_GPIO, LED_OFF_VALUE)

extern void Blink(unsigned char PIN, int DELAY_MS);

#define NotifyWifiConnected() LED_ON()

#define NotifyWifiConnecting() Blink(LED_GPIO, 3);

#define NotifyWifiNotConnected() LED_OFF()

#endif
