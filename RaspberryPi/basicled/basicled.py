#! /usr/bin/env python3

import sys
import os
import time
import paho.mqtt.client as mqtt #Python paho mqtt client. For more info https://www.eclipse.org/paho/clients/python/docs/
import RPi.GPIO as GPIO #Library to control the Raspberry Pi GPIO pins

#script version
version = "v1.0.1" 


# ======================================================================================================================================
#                                           Modify parameters below for your MQTT broker
# ======================================================================================================================================

topTopic = "/espio/"
mqttServerAddress = "192.168.255.10"
mqttServerPort = 1883
mqttClientId = 'pipwmledscript' #ensure the client id is unique
mqttUsername = "user"
mqttPassword = "password"
mqttUserPass = dict(username=mqttUsername, password=mqttPassword)

#======================================================================================================================================

dutyCycle = 50

#CONFIGURE GPIO
GPIO.setwarnings(False)
GPIO.setmode(GPIO.BOARD) #USING THE PHYSICAL PIN NUMBERING I.E. PIN 0 = PHYSICAL PIN 0 PIN 40 IS PHYSICAL PIN 40

GPIO.setup(12, GPIO.OUT)
pwm = GPIO.PWM(12, 100)
pwm.start(dutyCycle)
pinTopic = 'raspberrypi_gpio12'

# The callback when the client connects
def on_connect(client, userdata, flags, rc):
  if rc == 0:
    # Subscribing in on_connect() means that if we lose the connection and
    # reconnect then subscriptions will be renewed.
    client.subscribe(topTopic + pinTopic + "/cmnd/#")
    send_value(dutyCycle)
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
    send_value(dutyCycle)
  else:
    dutyCycle = int(msg.payload.decode('utf-8'))
    pwm.ChangeDutyCycle(dutyCycle)
    send_value(dutyCycle)

#Function to send the duty cycle value
def send_value(val):
  resultTopic = topTopic + pinTopic + "/stat/result"
  client.publish(resultTopic, payload='{"value":' +  str(val) + '}', qos=0, retain=False)

#client = mqtt.Client(client_id=mqttClientId, clean_session=True, userdata=None, transport="websockets") #use websockets
client = mqtt.Client(client_id=mqttClientId, clean_session=True, userdata=None, transport="tcp")
client.username_pw_set(mqttUsername, mqttPassword)
client.on_connect = on_connect
client.on_message = on_message
client.connect(mqttServerAddress, mqttServerPort, 60)

try:
  client.loop_forever()

except KeyboardInterrupt:
  pwm.stop()
  GPIO.cleanup()
