#! /usr/bin/env python3

import os
import threading
import time
import traceback
import paho.mqtt.publish as publish
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
  topic = MQTT_CLIENT_CODE + "/picputemp/sensor/update"
  payload = '{"value":' + str(int(int(cputemp)/1000)).strip() + '}'
  payload = payload.replace('\r', '').replace('\n', '')
  doPublish(topic, payload)


def diskUsed():
  usedspace = os.popen("df --output=pcent | awk -F'%' 'NR==2{print $1}'").read()
  topic = MQTT_CLIENT_CODE + "/pidiskused/sensor/update"
  payload = '{"value":' + str(usedspace).strip() + '}'
  payload = payload.replace('\r', '').replace('\n', '')
  doPublish(topic, payload)


def ramFree():
  raminfo = getRAMinfo()
  ramfree = int(raminfo[2])
  ramfreemb = ramfree / 1024
  topic = MQTT_CLIENT_CODE + "/pimemfree/sensor/update"
  payload = '{"value":' + str(ramfreemb) + '}'
  doPublish(topic, payload)

 
def doPublish(topic, payload):

  tls = {
    'cert_reqs' : ssl.CERT_REQUIRED,
    'tls_version' : ssl.PROTOCOL_TLSv1_2
  }

  publish.single(
    topic=topic,
    payload=payload,
    hostname=MQTT_SERVER,
    port=MQTT_PORT,
    retain=False,
    auth=MQTT_USER_PASS,
    qos=0,
    tls=tls,
    client_id=MQTT_CLIENT_ID
  )

threading.Thread(target=lambda: every(60, cpuTemp)).start()
threading.Thread(target=lambda: every(60, diskUsed)).start()
threading.Thread(target=lambda: every(60, ramFree)).start()
