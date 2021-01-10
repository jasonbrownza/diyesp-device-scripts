#! /usr/bin/env python3

import os
import threading
import time
import traceback
import paho.mqtt.client as mqtt
import ssl

# ======================================================================================================================================
#                                           Modify parameters below
# ======================================================================================================================================

MQTT_CLIENT_CODE = "REPLACE_WITH_YOUR_CLIENTCODE"; # To get your client code in the web app go to management -> settings 
MQTT_SERVER = "diyesp.com"
MQTT_PORT = 8883
MQTT_CLIENT_ID = MQTT_CLIENT_CODE + '_pihealth' #ensure the client id is unique
MQTT_USER = "REPLACE_WITH_YOUR_MQTT_USERNAME" # Your mqtt username (to get your username and password in the web app go to management -> settings)
MQTT_PASS = "REPLACE_WITH_YOUR_MQTT_PASSWORD" # Your mqtt password

#======================================================================================================================================

MQTT_USER_PASS = dict(username=MQTT_USER, password=MQTT_PASS)

# Return RAM information (unit=kb) in a list
# Index 0: total RAM
# Index 1: used RAM
# Index 2: free RAM
def getRAMinfo():
  cmd = os.popen('free')
  i = 0
  while 1:
    i = i + 1
    line = cmd.readline()
    if i==2:
      return(line.split()[1:4])


def cpuTemp():
  cputemp = os.popen("cat /sys/class/thermal/thermal_zone0/temp").read()
  payload = '{"value":' + str(int(int(cputemp)/1000)).strip() + '}'
  payload = payload.replace('\r', '').replace('\n', '')
  return payload


def diskUsed():
  usedspace = os.popen("df --output=pcent | awk -F'%' 'NR==2{print $1}'").read()
  payload = '{"value":' + str(usedspace).strip() + '}'
  payload = payload.replace('\r', '').replace('\n', '')
  return payload


def ramFree():
  raminfo = getRAMinfo()
  total = int(raminfo[0])
  free = int(raminfo[2])
  used =  round( (total - free) / total * 100, 2 )
  freePerc = float(100) - float(used)
  payload = '{"value":' + str(freePerc) + '}'
  return payload

next_reading = time.time()

client = mqtt.Client(client_id=MQTT_CLIENT_ID, clean_session=True, userdata=None, transport="tcp")
client.username_pw_set(MQTT_USER, MQTT_PASS)
client.tls_set (cert_reqs=ssl.CERT_REQUIRED, tls_version=ssl.PROTOCOL_TLSv1_2)
client.tls_insecure_set (False)
client.connect(MQTT_SERVER, MQTT_PORT, 60)

client.loop_start()

try:
  while True:
    # CPU Temperature
    topic = MQTT_CLIENT_CODE + "/picputemp/sensor/update"
    payload = cpuTemp()
    client.publish(topic, payload, qos=0, retain=False)
    time.sleep(0.02)

    # Disk Used Percentage
    topic = MQTT_CLIENT_CODE + "/pidiskused/sensor/update"
    payload = diskUsed()
    client.publish(topic, payload, qos=0, retain=False)
    time.sleep(0.02)

    # Free Memory MB
    topic = MQTT_CLIENT_CODE + "/pimemfree/sensor/update"
    payload = ramFree()
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

