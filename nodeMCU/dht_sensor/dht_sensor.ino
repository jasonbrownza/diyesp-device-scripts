#include <PubSubClient.h> //Requires PubSubClient found here: https://github.com/knolleary/pubsubclient
#include <ESP8266WiFi.h>
#include <Ticker.h>
#include "DHT.h"

/*
  ======================================================================================================================================
                                            Modify parameters below for your WiFi network and MQTT broker
  ======================================================================================================================================
*/

#define WIFI_SSID "REPLACE_WITH_YOUR_SSID"       // Your WiFi ssid
#define WIFI_PASS "REPLACE_WITH_YOUR_PASSWORD"    // Your WiFi password

#define MQTT_SERVER "REPLACE_WITH_YOUR_MQTT_BROKER_IP"   // Your mqtt server ip address
#define MQTT_PORT 1883             // Your mqtt port
#define MQTT_USER "user"      // mqtt username
#define MQTT_PASS "password"      // mqtt password

String MQTT_TOP_TOPIC = "/espio/";
String DEVICENAME = ""; //Must be unique amongst your devices. leave blank for automatic generation of unique name
String VERSION = "v1.0.1";
/*
  ======================================================================================================================================
*/

DHT dht;
const int dht11Pin = D1;
const String temperatureTopic = "temperature"; //Must be unique across all your devices
const String humidityTopic = "humidity"; //Must be unique across all your devices

long lastSensorPublish = 0;

WiFiClient wifiClient;

void cbMsgRec(char* topic, byte* payload, unsigned int length);
PubSubClient client(MQTT_SERVER, MQTT_PORT, cbMsgRec, wifiClient);

const int RSSI_MAX = -50; // define maximum strength of signal in dBm
const int RSSI_MIN = -100; // define minimum strength of signal in dBm

Ticker tickerDeviceInfo;

String DEVICEMACADDR;
String PLATFORM = ARDUINO_BOARD;

void setup() {

  dht.setup(dht11Pin);

  //WIFI
  WiFi.mode(WIFI_STA);

  uint8_t mac[6];
  WiFi.macAddress(mac);
  DEVICEMACADDR = macToStr(mac);
  if (DEVICENAME == "") {
    DEVICENAME = "espio-";
    DEVICENAME += DEVICEMACADDR;
  }

  PLATFORM = ARDUINO_BOARD;

  WiFi.hostname((char*) DEVICENAME.c_str());
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  reconnect();

  delay(500);

  tickerDeviceInfo.attach(60, doDevicePublish); //Publish device info every 60 seconds

}

void loop() {
  if (!client.connected() && WiFi.status() == 3) {
    reconnect();
  }

  doSensorPublish();

  //maintain MQTT connection
  client.loop();
}

void cbMsgRec(char* topic, byte* payload, unsigned int length) {

  //convert topic to string to make it easier to work with
  String topicStr = topic;
  String payloadStr = payloadString(payload, length);

  //Restart the ESP
  if (payloadStr == "RESTART" || payloadStr == "restart") {
    ESP.restart();
  }

}

void reconnect() {

  if (WiFi.status() != WL_CONNECTED) {
    while (WiFi.status() != WL_CONNECTED) {
      delay(1000);
    }
  }

  if (WiFi.status() == WL_CONNECTED) {
    while (!client.connected()) {
      if (client.connect((char*) DEVICENAME.c_str(), MQTT_USER, MQTT_PASS)) {
        String rebootTopic = MQTT_TOP_TOPIC + DEVICENAME + String("/restart");
        client.subscribe(rebootTopic.c_str());

        String connmsg = "{\"type\":2,\"msg\":\"" + DEVICENAME + " connected\"}";
        client.publish("/myhome/alerts", connmsg.c_str());

        doDevicePublish();
      }
    }
  }
}

void doDevicePublish() {
  int rssi = WiFi.RSSI();
  int signalPercentage = dBmtoPercentage(rssi);
  String payload = "{\"macaddr\":\"" + DEVICEMACADDR + "\",\"ip\":\"" + WiFi.localIP().toString() + "\",\"platform\":\"" + PLATFORM + "\",\"name\":\"" + DEVICENAME + "\",\"wifi_dBM\":" + String(rssi) + ",\"wifi_strength\":" + String(signalPercentage) + ",\"version\":\"" + VERSION + "\"}";
  String topic = MQTT_TOP_TOPIC + String("devices/info");
  client.publish(topic.c_str(), payload.c_str());
}

void doSensorPublish() {
  long now = millis();
  if (now - lastSensorPublish > 10000) {
    lastSensorPublish = now;

    String temperatureFullTopic = MQTT_TOP_TOPIC + temperatureTopic + String("/stat/result");
    String humidityFullTopic = MQTT_TOP_TOPIC + humidityTopic + String("/stat/result");

    float humidity = dht.getHumidity();/* Get humidity value */
    float temperature = dht.getTemperature();/* Get temperature value */

    String temperaturePayload = "{\"value\":" + String(temperature) + "}";
    client.publish(temperatureFullTopic.c_str(), temperaturePayload.c_str());

    String humidityPayload = "{\"value\":" + String(humidity) + "}";
    client.publish(humidityFullTopic.c_str(), humidityPayload.c_str());
  }
}

//generate unique name from MAC addr
String macToStr(const uint8_t* mac) {
  String result;
  for (int i = 0; i < 6; ++i) {
    result += String(mac[i], 16);
    if (i < 5) {
      result += ':';
    }
  }
  return result;
}

int dBmtoPercentage(int dBm) {
  int quality;
  if (dBm <= RSSI_MIN)
  {
    quality = 0;
  }
  else if (dBm >= RSSI_MAX)
  {
    quality = 100;
  }
  else
  {
    quality = 2 * (dBm + 100);
  }
  return quality;
}

String payloadString(byte* payload, unsigned int payload_len) {
  String str;
  str.reserve(payload_len);
  for (uint32_t i = 0; i < payload_len; i++)
    str += (char)payload[i];
  return str;
}
