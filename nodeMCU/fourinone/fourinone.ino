#include <PubSubClient.h> //Requires PubSubClient found here: https://github.com/knolleary/pubsubclient
#include <ESP8266WiFi.h>
#include <Ticker.h>
#include "DHT.h"

/*
  ======================================================================================================================================
                                            Modify parameters below for your WiFi network and MQTT broker
  ======================================================================================================================================
*/

#define WIFI_SSID "outside"       // Your WiFi ssid
#define WIFI_PASS "rodrigues21"    // Your WiFi password

#define MQTT_SERVER "192.168.255.10"   // Your mqtt server ip address
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
int ledState;
const String ledTopic = "basicled";

const int led2Pin = D2;
int ledValue = 500;
int ledPercent; // stores the percentage equivalent of ledValue
const String led2Topic = "pwmled"; //Must be unique across all your devices

// doorContact
const int doorContactPin = D5;
int doorContactState;
const String doorContactTopic = "doorcontact";
long lastDhtPublish = 0;

DHT dht;
const int dht11Pin = D6;
const String temperatureTopic = "temperature"; //Must be unique across all your devices
const String humidityTopic = "humidity"; //Must be unique across all your devices


WiFiClient wifiClient;

void cbMsgRec(char* topic, byte* payload, unsigned int length);
PubSubClient client(MQTT_SERVER, MQTT_PORT, cbMsgRec, wifiClient);

const int RSSI_MAX = -50; // define maximum strength of signal in dBm
const int RSSI_MIN = -100; // define minimum strength of signal in dBm

Ticker tickerStatusUpdate;
Ticker tickerDeviceInfo;

String DEVICEMACADDR;
String PLATFORM = ARDUINO_BOARD;

void setup() {

  pinMode(ledPin, OUTPUT);

  pinMode(doorContactPin, INPUT);
  doorContactState = !digitalRead(doorContactPin);

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

  tickerStatusUpdate.attach(30, doDoorcPublish); //Publish the status of the door contact every 30 seconds
  tickerDeviceInfo.attach(60, doDevicePublish); //Publish device info every 60 seconds

}

void loop() {
  if (!client.connected() && WiFi.status() == 3) {
    reconnect();
  }

  if (doorContactState != !digitalRead(doorContactPin)) {
    toggledoorContact();
  }

  doDhtPublish();

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

  //Handle turning on/off the led
  String ledCmndFullTopic = MQTT_TOP_TOPIC + ledTopic + String("/cmnd/power");
  String ledResultFullTopic = MQTT_TOP_TOPIC + ledTopic + String("/stat/result");
  if (strcmp(topic, ledCmndFullTopic.c_str()) == 0) {
    if (payloadStr == "ON" || payloadStr == "on") {
      digitalWrite(ledPin, HIGH);
      ledState = 1;
      char* resultPayload = "{\"power\":\"on\"}";
      client.publish(ledResultFullTopic.c_str(), resultPayload);
    } else if (payloadStr == "OFF" || payloadStr == "off") {
      digitalWrite(ledPin, LOW);
      ledState = 0;
      char* resultPayload = "{\"power\":\"off\"}";
      client.publish(ledResultFullTopic.c_str(), resultPayload);
    } else {
      if (ledState) {
        char* resultPayload = "{\"power\":\"on\"}";
        client.publish(ledResultFullTopic.c_str(), resultPayload);
      } else {
        char* resultPayload = "{\"power\":\"off\"}";
        client.publish(ledResultFullTopic.c_str(), resultPayload);
      }
    }
  }

  //Handle the brightness of the led
  String led2CmndFullTopic = MQTT_TOP_TOPIC + led2Topic + String("/cmnd/value");
  String led2ResultFullTopic = MQTT_TOP_TOPIC + led2Topic + String("/stat/result");
  if (strcmp(topic, led2CmndFullTopic.c_str()) == 0) {
    if (payloadStr == "?") {
      ledPercent = map(ledValue, 0, 1023, 0, 100);
      String resultPayload = "{\"value\":" + String(ledPercent) + "}";
      client.publish(led2ResultFullTopic.c_str(), resultPayload.c_str());
    } else {
      ledPercent = payloadStr.toInt();
      ledValue = map(ledPercent, 0, 100, 0, 1023);
      analogWrite(led2Pin, ledValue);
      String resultPayload = "{\"value\":" + payloadStr + "}";
      client.publish(led2ResultFullTopic.c_str(), resultPayload.c_str());
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
        
        String doorContactCmndFullTopic = MQTT_TOP_TOPIC + doorContactTopic + String("/cmnd/stat");
        client.subscribe(doorContactCmndFullTopic.c_str());

        String connmsg = "{\"type\":2,\"msg\":\"" + DEVICENAME + " connected\"}";
        client.publish("/myhome/alerts", connmsg.c_str());
        
        String ledCmndFullTopic = MQTT_TOP_TOPIC + ledTopic + String("/cmnd/power");
        client.subscribe(ledCmndFullTopic.c_str());
        
        String led2CmndFullTopic = MQTT_TOP_TOPIC + led2Topic + String("/cmnd/value");
        client.subscribe(led2CmndFullTopic.c_str());

        String ledResultFullTopic = MQTT_TOP_TOPIC + ledTopic + String("/stat/result");
        if (ledState) {
          char* resultPayload = "{\"power\":\"on\"}";
          client.publish(ledResultFullTopic.c_str(), resultPayload);
        } else {
          char* resultPayload = "{\"power\":\"off\"}";
          client.publish(ledResultFullTopic.c_str(), resultPayload);
        }

        String led2ResultFullTopic = MQTT_TOP_TOPIC + led2Topic + String("/stat/result");
        ledPercent = map(ledValue, 0, 1023, 0, 100);
        String resultPayload = "{\"value\":" + String(ledPercent) + "}";
        client.publish(led2ResultFullTopic.c_str(), resultPayload.c_str());

        doDoorcPublish();

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

void doDoorcPublish() {
  String doorContactResultFullTopic = MQTT_TOP_TOPIC + doorContactTopic + String("/stat/result");
  if (doorContactState) {
    char* resultPayload = "{\"value\":1}";
    client.publish(doorContactResultFullTopic.c_str(), resultPayload);
  } else {
    char* resultPayload = "{\"value\":0}";
    client.publish(doorContactResultFullTopic.c_str(), resultPayload);
  }

}

void toggledoorContact() {
  String doorContactResultFullTopic = MQTT_TOP_TOPIC + doorContactTopic + String("/stat/result");
  if (doorContactState) {
    doorContactState = 0;
    char* resultPayload = "{\"value\":0}";
    client.publish(doorContactResultFullTopic.c_str(), resultPayload);
  } else {
    doorContactState = 1;
    char* resultPayload = "{\"value\":1}";
    client.publish(doorContactResultFullTopic.c_str(), resultPayload);

    //Send an alert when the door contact is open
    delay(10);
    String connmsg = "{\"type\":4,\"msg\":\"Door Opened\"}";
    client.publish("/myhome/alerts", connmsg.c_str());
  }
}

void doDhtPublish() {
  long now = millis();
  if (now - lastDhtPublish > 10000) {
    lastDhtPublish = now;

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
