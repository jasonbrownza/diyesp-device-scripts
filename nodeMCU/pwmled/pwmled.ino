#include <PubSubClient.h> //Requires PubSubClient found here: https://github.com/knolleary/pubsubclient
#include <ESP8266WiFi.h>
#include <WiFiClientSecure.h>
#include <EEPROM.h>
#include <time.h>

/*
  ======================================================================================================================================
                                            Modify parameters below for your WiFi network and MQTT broker
  ======================================================================================================================================
*/

#define WIFI_SSID "REPLACE_WITH_YOUR_SSID" // Your WiFi ssid
#define WIFI_PASS "REPLACE_WITH_YOUR_PASSWORD" // Your WiFi password

#define MQTT_SERVER "diyesp.com"
#define MQTT_PORT 8883 // Port 1883 can be used it is insecure and your username & password will be transmitted in plaintext. Use 8883 for encrypted connections
#define MQTT_USER "REPLACE_WITH_YOUR_MQTT_USERNAME" // Your mqtt username (to get your username and password in the web app go to management -> settings)
#define MQTT_PASS "REPLACE_WITH_YOUR_MQTT_PASSWORD" // Your mqtt password

String MQTT_CLIENT_CODE = "REPLACE_WITH_YOUR_CLIENTCODE"; // To get your client code in the web app go to management -> settings 
String DEVICENAME = ""; //Must be unique amongst your devices, the first 13 characters must be your MQTT_CLIENT_CODE. leave blank for automatic generation of unique name

/*
  ======================================================================================================================================
*/

// LED
const int ledPin = D1;
int ledValue = 0;
int ledPercent; // stores the percentage equivalent of ledValue
const String ledTopic = "pwmled"; //Must be unique across all your devices

WiFiClientSecure wifiClient;

void cbMsgRec(char* topic, byte* payload, unsigned int length);
PubSubClient client(MQTT_SERVER, MQTT_PORT, cbMsgRec, wifiClient);

String DEVICEMACADDR;
String PLATFORM = ARDUINO_BOARD;

void setup() {

  //Initialize serial and wait for port to open:
  Serial.begin(115200);
  delay(500);
  Serial.println("Starting...");

  EEPROM.begin(512);
  EEPROM.get(0, ledValue);
  analogWrite(ledPin, abs(int(ledValue)));
  Serial.println("Set initial value");
  Serial.println(ledValue);

  //WIFI
  WiFi.mode(WIFI_STA);
  wifiClient.setInsecure();

  uint8_t mac[6];
  WiFi.macAddress(mac);
  DEVICEMACADDR = macToStr(mac);
  if (DEVICENAME == "") {
    DEVICENAME = MQTT_CLIENT_CODE;
    DEVICENAME += DEVICEMACADDR;
  }

  PLATFORM = ARDUINO_BOARD;

  WiFi.hostname((char*) DEVICENAME.c_str());
  WiFi.begin(WIFI_SSID, WIFI_PASS);

  delay(500);
  
  reconnect();

  delay(500);

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

  //Handle the brightness of the led
  String ledCmndFullTopic = MQTT_CLIENT_CODE + "/" + ledTopic + "/device/value";
  String ledResultFullTopic = MQTT_CLIENT_CODE + "/" + ledTopic + "/device/update";
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
    Serial.print("Attempting to connect to SSID: ");
    Serial.println(WIFI_SSID);
    while (WiFi.status() != WL_CONNECTED) {
      Serial.print(".");
      delay(1000);
    }
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.print("WIFI connected to ");
    Serial.println(WIFI_SSID);
    Serial.println("IP address: ");
    Serial.println(WiFi.localIP());

    getTime();
    delay(500);
    
    Serial.println("Connecting to MQTT broker ");
    Serial.println(MQTT_SERVER);
    while (!client.connected()) {
      if (client.connect((char*) DEVICENAME.c_str(), MQTT_USER, MQTT_PASS)) {

        String ledCmndFullTopic = MQTT_CLIENT_CODE + "/" + ledTopic + "/device/value";
        client.subscribe(ledCmndFullTopic.c_str());

        String ledResultFullTopic = MQTT_CLIENT_CODE + "/" + ledTopic + "/device/update";
        ledPercent = map(ledValue, 0, 1023, 0, 100);
        String resultPayload = "{\"value\":" + String(ledPercent) + "}";
        client.publish(ledResultFullTopic.c_str(), resultPayload.c_str());
        delay(20);

      } else {
        Serial.print(".");
      }
    }
  }
}

void getTime() {
  Serial.print("Setting time using SNTP");
  configTime(8 * 3600, 0, "pool.ntp.org");
  time_t now = time(nullptr);
  while (now < 1000) {
    delay(500);
    Serial.print(".");
    now = time(nullptr);
  }
  Serial.println("");
  struct tm timeinfo;
  gmtime_r(&now, &timeinfo);
  Serial.print("Current time: ");
  Serial.print(asctime(&timeinfo));
}

//generate unique name from MAC addr
String macToStr(const uint8_t* mac) {
  String result;
  for (int i = 0; i < 6; ++i) {
    result += String(mac[i], 16);
  }
  return result;
}

String payloadString(byte* payload, unsigned int payload_len) {
  String str;
  str.reserve(payload_len);
  for (uint32_t i = 0; i < payload_len; i++)
    str += (char)payload[i];
  return str;
}
