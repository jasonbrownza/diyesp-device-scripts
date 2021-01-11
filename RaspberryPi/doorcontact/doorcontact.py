#! /usr/bin/env python3

import time
import paho.mqtt.client as mqtt #Python paho mqtt client. For more info https://www.eclipse.org/paho/clients/python/docs/
import RPi.GPIO as GPIO #Library to control the Raspberry Pi GPIO pins
import ssl
import threading
import traceback
import atexit

# ======================================================================================================================================
#                                           Modify parameters below
# ======================================================================================================================================

MQTT_CLIENT_CODE = "REPLACE_WITH_YOUR_CLIENTCODE"; # To get your client code in the web app go to management -> settings 
MQTT_SERVER = "diyesp.com"
MQTT_PORT = 8883
MQTT_CLIENT_ID = MQTT_CLIENT_CODE + '_ledpushbtn' #ensure the client id is unique
MQTT_USER = "REPLACE_WITH_YOUR_MQTT_USERNAME" # Your mqtt username (to get your username and password in the web app go to management -> settings)
MQTT_PASS = "REPLACE_WITH_YOUR_MQTT_PASSWORD" # Your mqtt password

#======================================================================================================================================

#CONFIGURE GPIO
GPIO.setwarnings(False)
GPIO.setmode(GPIO.BOARD) #USING THE PHYSICAL PIN NUMBERING I.E. PIN 0 = PHYSICAL PIN 0 PIN 40 IS PHYSICAL PIN 40

reedSwitchPin = 16
GPIO.setup(reedSwitchPin, GPIO.IN, pull_up_down=GPIO.PUD_UP)

reedSwitchState = GPIO.input(reedSwitchPin)

def button_callback(channel):
  time.sleep(0.01)
  global reedSwitchState
  sendStatus()

GPIO.add_event_detect(reedSwitchPin, GPIO.RISING, callback=button_callback, bouncetime=200)

def sendStatus():
  topic = MQTT_CLIENT_CODE + "/doorcontact/sensor/update"
  global reedSwitchState
  reedSwitchState = 0 if GPIO.input(reedSwitchPin) == 1 else 1
  payload = '{"value":' + str(reedSwitchState) + '}'
  client.publish(topic, payload=payload, qos=0, retain=False)

def heartbeat():
  topic = MQTT_CLIENT_CODE + "/doorcontact/sensor/heartbeat"
  global reedSwitchState
  reedSwitchState = 0 if GPIO.input(reedSwitchPin) == 1 else 1
  payload = '{"value":' + str(reedSwitchState) + '}'
  client.publish(topic, payload=payload, qos=0, retain=False)

def exit_handler():
  client.loop_stop()
  client.disconnect()
  print("Exit")

def every(delay, task):
  next_time = time.time() + delay
  while True:
    time.sleep(max(0, next_time - time.time()))
    try:
      task()
    except Exception:
      err = traceback.print_exc()
      #traceback.print_exc()
      # in production code you might want to have this instead of course:
      # logger.exception("Problem while executing repetitive task.")
    # skip tasks if we are behind schedule:
    next_time += (time.time() - next_time) // delay * delay + delay

atexit.register(exit_handler)

#client = mqtt.Client(client_id=mqttClientId, clean_session=True, userdata=None, transport="websockets") #use websockets
client = mqtt.Client(client_id=MQTT_CLIENT_ID, clean_session=True, userdata=None, transport="tcp")
client.username_pw_set(MQTT_USER, MQTT_PASS)
client.tls_set (cert_reqs=ssl.CERT_REQUIRED, tls_version=ssl.PROTOCOL_TLSv1_2)
client.tls_insecure_set (False)
client.connect(MQTT_SERVER, MQTT_PORT, 60)
client.loop_start()

threading.Thread(target=lambda: every(60, heartbeat)).start()
