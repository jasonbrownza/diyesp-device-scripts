#include <PubSubClient.h> //Requires PubSubClient found here: https://github.com/knolleary/pubsubclient
#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>
#include <Ticker.h>
//Requires IRremoteESP8266 found here: https://github.com/crankyoldgit/IRremoteESP8266
#include <IRremoteESP8266.h>
#include <IRsend.h>

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

String irRecTopic = "irtransmitter";
IRsend irsend(3);

WiFiClient wifiClient;

void cbMsgRec(char* topic, byte* payload, unsigned int length);
PubSubClient client(MQTT_SERVER, MQTT_PORT, cbMsgRec, wifiClient);

long lastInfoPublish = 0;

const int RSSI_MAX = -50; // define maximum strength of signal in dBm
const int RSSI_MIN = -100; // define minimum strength of signal in dBm

String DEVICEMACADDR;

Ticker tickerDeviceInfo;

void setup() {

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

  irsend.begin();
  
  tickerDeviceInfo.attach(60, doDevicePublish); //Publish device info every 60 seconds
}

void loop() {
  if (!client.connected() && WiFi.status() == 3) {
    reconnect();
  }

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

  if (payloadStr == "power") {
    irsend.sendRC5(0x80C, 12);
  }

  if (payloadStr == "mute") {
    irsend.sendRC5(0x838, 12);
  }

  if (payloadStr == "volup") {
    irsend.sendRC5(0x810, 12);
  }

  if (payloadStr == "voldown") {
    irsend.sendRC5(0x811, 12);
  }

  //Reply back with the last command executed
  String irResultFullTopic = MQTT_TOP_TOPIC + irRecTopic + String("/stat/result");
  client.publish(irResultFullTopic.c_str(), payloadStr.c_str());
  
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

        String irFullTopic = MQTT_TOP_TOPIC + irRecTopic + String("/cmnd/transmit");
        client.subscribe(irFullTopic.c_str());

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