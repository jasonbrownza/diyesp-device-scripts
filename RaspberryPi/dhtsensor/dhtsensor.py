#! /usr/bin/env python3

import time
import paho.mqtt.client as mqtt #Python paho mqtt client. For more info https://www.eclipse.org/paho/clients/python/docs/
import RPi.GPIO as GPIO #Library to control the Raspberry Pi GPIO pins
import ssl
import Adafruit_DHT

# ======================================================================================================================================
#                                           Modify parameters below
# ======================================================================================================================================

MQTT_CLIENT_CODE = "REPLACE_WITH_YOUR_CLIENTCODE"; # To get your client code in the web app go to management -> settings 
MQTT_SERVER = "diyesp.com"
MQTT_PORT = 8883
MQTT_CLIENT_ID = MQTT_CLIENT_CODE + '_dht' #ensure the client id is unique
MQTT_USER = "REPLACE_WITH_YOUR_MQTT_USERNAME" # Your mqtt username (to get your username and password in the web app go to management -> settings)
MQTT_PASS = "REPLACE_WITH_YOUR_MQTT_PASSWORD" # Your mqtt password

#======================================================================================================================================

#CONFIGURE GPIO
GPIO.setwarnings(False)
GPIO.setmode(GPIO.BOARD) #USING THE PHYSICAL PIN NUMBERING I.E. PIN 0 = PHYSICAL PIN 0 PIN 40 IS PHYSICAL PIN 40

dhtPin = 4
dhtSensor = Adafruit_DHT.DHT11

next_reading = time.time()

client = mqtt.Client(client_id=MQTT_CLIENT_ID, clean_session=True, userdata=None, transport="tcp")
client.username_pw_set(MQTT_USER, MQTT_PASS)
client.tls_set (cert_reqs=ssl.CERT_REQUIRED, tls_version=ssl.PROTOCOL_TLSv1_2)
client.tls_insecure_set (False)
client.connect(MQTT_SERVER, MQTT_PORT, 60)

client.loop_start()

try:
  while True:
    humidity, temperature = Adafruit_DHT.read(dhtSensor, dhtPin)

    # Publish humidity
    topic = MQTT_CLIENT_CODE + "/humidity/sensor/update"
    payload = '{"value":' + str(humidity) + '}'
    client.publish(topic, payload, qos=0, retain=False)
    time.sleep(0.02)

    # Publish temperature
    topic = MQTT_CLIENT_CODE + "/temperature/sensor/update"
    payload = '{"value":' + str(temperature) + '}'
    client.publish(topic, payload, qos=0, retain=False)

    # Publish sensors every 60 seconds
    next_reading += 60
    sleep_time = next_reading-time.time()
    if sleep_time > 0:
      time.sleep(sleep_time)

except KeyboardInterrupt:
    pass

client.loop_stop()
client.disconnect()

