#include <PubSubClient.h> //Requires PubSubClient found here: https://github.com/knolleary/pubsubclient
#include <ESP8266WiFi.h>
#include <Ticker.h>
#include <EEPROM.h>

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


// LED
const int ledPin = D1;
int ledValue = 0;
int ledPercent; // stores the percentage equivalent of ledValue
const String ledTopic = "pwmled"; //Must be unique across all your devices

WiFiClient wifiClient;

void cbMsgRec(char* topic, byte* payload, unsigned int length);
PubSubClient client(MQTT_SERVER, MQTT_PORT, cbMsgRec, wifiClient);

const int RSSI_MAX = -50; // define maximum strength of signal in dBm
const int RSSI_MIN = -100; // define minimum strength of signal in dBm

Ticker tickerDeviceInfo;

String DEVICEMACADDR;
String PLATFORM = ARDUINO_BOARD;

void setup() {

  EEPROM.begin(512);
  EEPROM.get(0, ledValue);
  analogWrite(ledPin, ledValue);

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

  //Handle the brightness of the led
  String ledCmndFullTopic = MQTT_TOP_TOPIC + ledTopic + String("/cmnd/value");
  String ledResultFullTopic = MQTT_TOP_TOPIC + ledTopic + String("/stat/result");
  if (strcmp(topic, ledCmndFullTopic.c_str()) == 0) {
    if (payloadStr == "?") {
      ledPercent = map(ledValue, 0, 1023, 0, 100);
      String resultPayload = "{\"value\":" + String(ledPercent) + "}";
      client.publish(ledResultFullTopic.c_str(), resultPayload.c_str());
    } else {
      ledPercent = payloadStr.toInt();
      ledValue = map(ledPercent, 0, 100, 0, 1023);
      analogWrite(ledPin, ledValue);
      String resultPayload = "{\"value\":" + payloadStr + "}";
      client.publish(ledResultFullTopic.c_str(), resultPayload.c_str());

      //save value to eeprom
      EEPROM.begin(512);
      int eeAddress = 0;
      EEPROM.put(eeAddress, ledValue);
      eeAddress += sizeof(int);
      EEPROM.put(0, ledValue);
      EEPROM.commit();
    }
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
        String ledCmndFullTopic = MQTT_TOP_TOPIC + ledTopic + String("/cmnd/value");
        client.subscribe(ledCmndFullTopic.c_str());

        String connmsg = "{\"type\":2,\"msg\":\"" + DEVICENAME + " connected\"}";
        client.publish("/myhome/alerts", connmsg.c_str());
        delay(20);

        String ledResultFullTopic = MQTT_TOP_TOPIC + ledTopic + String("/stat/result");
        ledPercent = map(ledValue, 0, 1023, 0, 100);
        String resultPayload = "{\"value\":" + String(ledPercent) + "}";
        client.publish(ledResultFullTopic.c_str(), resultPayload.c_str());
        delay(20);

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
