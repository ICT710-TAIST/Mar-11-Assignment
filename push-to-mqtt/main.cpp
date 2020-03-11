#include <mbed.h>
#include <MQTTClientMbedOs.h>
#include "GapScanner.h"

EventQueue event_queue;

DigitalOut led(LED1);
InterruptIn button(USER_BUTTON);
WiFiInterface *wifi;
TCPSocket* socket;
MQTTClient* mqttClient;
Thread thread;
EventQueue queue(5 * EVENTS_EVENT_SIZE);
GapScanner* scanner;

const char CLIENT_ID[] = "4665fab9-4827-40de-a1a6-36e538463bc4";
const char NETPIE_TOKEN[] = "CXhbMLgUwHFZWKdt77AHEVAgio42f3k7"; 
const char MQTT_TOPIC[] = "@msg/taist2020/button/2";

char*  ble_addr = "F4:5C:89:92:12:34";

void pressed_handler() {

    int ret;
    int rssi;

    MQTT::Message message;
    char buf[100];

    while(1) {
        if ((rssi = scanner->get_rssi())!=0) {
            sprintf(buf, "{\"rssi\":%d}", rssi);
            message.qos = MQTT::QOS0;
            message.retained = false;
            message.dup = false;
            message.payload = (void*)buf;
            message.payloadlen = strlen(buf)+1;
            ret = mqttClient->publish(MQTT_TOPIC, message);
            if (ret != 0) {
                printf("rc from publish was %d\r\n", ret);
                return;
            }
            printf("Published topic: %s\t msg: %s", MQTT_TOPIC, buf);
        }
        ThisThread::sleep_for(500);
    }
}

void schedule_ble_events(BLE::OnEventsToProcessCallbackContext *context) {
    event_queue.call(Callback<void()>(&context->ble, &BLE::processEvents));
}
 
int main() { 
    // Bluetooth connection 
    BLE &ble = BLE::Instance();
    ble.onEventsToProcess(schedule_ble_events);
    scanner = new GapScanner(ble, event_queue, ble_addr);

    // WiFi connection
    wifi = WiFiInterface::get_default_instance();
    if (!wifi) {
        printf("ERROR: No WiFiInterface found.\n");
        return -1;
    }
    int ret = wifi->connect(MBED_CONF_APP_WIFI_SSID, MBED_CONF_APP_WIFI_PASSWORD, NSAPI_SECURITY_WPA_WPA2);
    if (ret != 0) {
        printf("\nConnection error: %d\n", ret);
        return -1;
    }

    // Socket connection
    socket = new TCPSocket();
    socket->open(wifi);
    SocketAddress addr;
    wifi->gethostbyname("mqtt.netpie.io", &addr);
    addr.set_port(1883);
    socket->connect(addr);
    if (ret != 0) {
        printf("rc from TCP connect is %d\r\n", ret);
        return -1;
    }

    // MQTT connection
    mqttClient = new MQTTClient(socket); 
    MQTTPacket_connectData data = MQTTPacket_connectData_initializer;
    //data.MQTTVersion = 3;
    data.clientID.cstring = (char*)CLIENT_ID;
    data.username.cstring = (char*)NETPIE_TOKEN;
    //data.password.cstring = (char*)NETPIE_SECRET;
    ret = mqttClient->connect(data);
    if (ret != 0) {
        printf("rc from MQTT connect is %d\r\n", ret);
        return -1;
    }

    thread.start(callback(&queue, &EventQueue::dispatch_forever));
    button.fall(queue.event(pressed_handler));

    scanner->run();

    // printf("Starting\n");
    // while(1) {
    //     led = !led;
    //     ThisThread::sleep_for(500);
    // }
}
