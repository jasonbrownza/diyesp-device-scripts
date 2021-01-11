#! /usr/bin/env python3

import time
import paho.mqtt.client as mqtt #Python paho mqtt client. For more info https://www.eclipse.org/paho/clients/python/docs/
import RPi.GPIO as GPIO #Library to control the Raspberry Pi GPIO pins
import ssl

# ======================================================================================================================================
#                                           Modify parameters below
# ======================================================================================================================================

MQTT_CLIENT_CODE = "REPLACE_WITH_YOUR_CLIENTCODE"; # To get your client code in the web app go to management -> settings 
MQTT_SERVER = "diyesp.com"
MQTT_PORT = 8883
MQTT_CLIENT_ID = MQTT_CLIENT_CODE + '_basicled' #ensure the client id is unique
MQTT_USER = "REPLACE_WITH_YOUR_MQTT_USERNAME" # Your mqtt username (to get your username and password in the web app go to management -> settings)
MQTT_PASS = "REPLACE_WITH_YOUR_MQTT_PASSWORD" # Your mqtt password

#======================================================================================================================================

#CONFIGURE GPIO
GPIO.setwarnings(False)
GPIO.setmode(GPIO.BOARD) #USING THE PHYSICAL PIN NUMBERING I.E. PIN 0 = PHYSICAL PIN 0 PIN 40 IS PHYSICAL PIN 40

ledPin = 12
GPIO.setup(ledPin, GPIO.OUT)

# The callback when the client connects
def on_connect(client, userdata, flags, rc):
  if rc == 0:
    # Subscribing in on_connect() means that if we lose the connection and
    # reconnect then subscriptions will be renewed.
    client.subscribe(MQTT_CLIENT_CODE + "/basicled/device/power")
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
    sendStatus()
  else:
    setGpio(msg.payload.decode('utf-8'))

#Function to set the PIN on or off
def setGpio(payload):
  topic = MQTT_CLIENT_CODE + "/basicled/device/update"
  if payload.lower() == 'on':
    GPIO.output(ledPin,True)
    client.publish(topic, payload='{"power":"on"}', qos=0, retain=False)
  if payload.lower() == 'off':
    GPIO.output(ledPin,False)
    client.publish(topic, payload='{"power":"off"}', qos=0, retain=False)

#Function to send the PIN / LED status i.e. is the led on or off
def sendStatus():
  topic = MQTT_CLIENT_CODE + "/basicled/device/update"
  ledState = GPIO.input(ledPin)
  if ledState == 1:
    payload = '{"power":"on"}'
  else:
    payload = '{"power":"off"}'
  client.publish(topic, payload=payload, qos=0, retain=False)

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
  GPIO.cleanup()
