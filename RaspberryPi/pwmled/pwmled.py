#! /usr/bin/env python3

import sys
import os
import time
import paho.mqtt.client as mqtt #Python paho mqtt client. For more info https://www.eclipse.org/paho/clients/python/docs/
import RPi.GPIO as GPIO #Library to control the Raspberry Pi GPIO pins
import ssl

# ======================================================================================================================================
#                                           Modify parameters below for your MQTT broker
# ======================================================================================================================================

MQTT_CLIENT_CODE = "REPLACE_WITH_YOUR_CLIENTCODE"; # To get your client code in the web app go to management -> settings 
MQTT_SERVER = "diyesp.com"
MQTT_PORT = 8883
MQTT_CLIENT_ID = MQTT_CLIENT_CODE + '_basicled' #ensure the client id is unique
MQTT_USER = "REPLACE_WITH_YOUR_MQTT_USERNAME" # Your mqtt username (to get your username and password in the web app go to management -> settings)
MQTT_PASS = "REPLACE_WITH_YOUR_MQTT_PASSWORD" # Your mqtt password

#======================================================================================================================================

dutyCycle = 50

#CONFIGURE GPIO
GPIO.setwarnings(False)
GPIO.setmode(GPIO.BOARD) #USING THE PHYSICAL PIN NUMBERING I.E. PIN 0 = PHYSICAL PIN 0 PIN 40 IS PHYSICAL PIN 40

GPIO.setup(12, GPIO.OUT)
pwm = GPIO.PWM(12, 100)
pwm.start(dutyCycle)
pinTopic = 'pwmled'

# The callback when the client connects
def on_connect(client, userdata, flags, rc):
  if rc == 0:
    # Subscribing in on_connect() means that if we lose the connection and
    # reconnect then subscriptions will be renewed.
    client.subscribe(MQTT_CLIENT_CODE + "/pwmled/device/value")
    send_value()
  elif rc == 1:
    sys.exit("Connection refused: incorrect protocol version") 
  elif rc == 2:
    sys.exit("Connection refused: invalid client identifier") 
  elif rc == 3:
    sys.exit("Connection refused: server unavailable") 
  elif rc == 4:
    sys.exit("Connection refused: bad username or password")
  elif rc == 5:
    sys.exit("Connection refused: not authorised")
  else :
    sys.exit("Connection refused: Return code " + str(rc))
      

# The callback for when a PUBLISH message is received
def on_message(client, userdata, msg):
  if msg.payload.decode('utf-8') == "?":
    send_value()
  else:
    global dutyCycle
    dutyCycle = int(msg.payload.decode('utf-8'))
    pwm.ChangeDutyCycle(dutyCycle)
    send_value()

#Function to send the duty cycle value
def send_value():
  resultTopic = MQTT_CLIENT_CODE + "/pwmled/device/update"
  client.publish(resultTopic, payload='{"value":' +  str(dutyCycle) + '}', qos=0, retain=False)

#client = mqtt.Client(client_id=mqttClientId, clean_session=True, userdata=None, transport="websockets") #use websockets
client = mqtt.Client(client_id=MQTT_CLIENT_ID, clean_session=True, userdata=None, transport="tcp")
client.username_pw_set(MQTT_USER, MQTT_PASS)
client.on_connect = on_connect
client.on_message = on_message
client.tls_set (cert_reqs=ssl.CERT_REQUIRED, tls_version=ssl.PROTOCOL_TLSv1_2)
client.tls_insecure_set (False)
client.connect(MQTT_SERVER, MQTT_PORT, 60)

try:
  client.loop_forever()

except KeyboardInterrupt:
  pwm.stop()
  GPIO.cleanup()
