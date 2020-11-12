#! /usr/bin/env python3

import os
import threading
import time
import traceback
import paho.mqtt.publish as publish

#script version
version = "v1.0.1"


# ======================================================================================================================================
#                                           Modify parameters below for your MQTT broker
# ======================================================================================================================================

mqttServerAddress = "192.168.255.10"
mqttServerPort = 1883
mqttClientId = 'pihealthscript' #ensure the client id is unique
mqttUsername = "user"
mqttPassword = "password"
mqttUserPass = dict(username=mqttUsername, password=mqttPassword)

#======================================================================================================================================

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

def deviceInfo():
  macaddr = os.popen("cat /sys/class/net/wlan0/address").read()
  device = os.popen("hostname").read()
  pimodel = os.popen("cat /proc/cpuinfo | grep Model").read().split(":")
  platform = pimodel[1]
  ip = os.popen("ifconfig wlan0 | grep inet | awk '{ print $2 }'").readline()
  dbm = os.popen("iwconfig wlan0 | grep -i level | awk '{ print $4 }'").read().replace("level=", "")
  quality = 2 * (int(dbm) + 100)
  topic = "/myhome/devices/info"
  payload = '{"ip":"' + str(ip) + '","macaddr":"' + macaddr + '","name":"' + device + '","platform":"' + platform + '","wifi_dBM":' + str(dbm) + ',"wifi_strength":' + str(quality) + '}'
  payload = payload.replace('\r', '').replace('\n', '')
  doPublish(topic, payload)


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
  topic = "/myhome/picputemp/stat/result"
  payload = '{"value":' + str(int(int(cputemp)/1000)) + '}'
  payload = payload.replace('\r', '').replace('\n', '')
  doPublish(topic, payload)

def diskUsed():
  usedspace = os.popen("df --output=pcent | awk -F'%' 'NR==2{print $1}'").read()
  topic = "/myhome/pidiskused/stat/result"
  payload = '{"value":' + str(usedspace).strip() + '}'
  payload = payload.replace('\r', '').replace('\n', '')
  doPublish(topic, payload)

def ramFree():
  raminfo = getRAMinfo()
  ramfree = int(raminfo[2])
  ramfreemb = ramfree / 1024
  topic = "/myhome/pimemfree/stat/result"
  payload = '{"value":' + str(ramfreemb) + '}'
  doPublish(topic, payload)
  
def doPublish(topic, payload):
  publish.single(
    topic=topic,
    payload=payload,
    hostname=mqttServerAddress,
    port=mqttServerPort,
    retain=False,
    auth=mqttUserPass,
    qos=0
  )

threading.Thread(target=lambda: every(60, deviceInfo)).start()
threading.Thread(target=lambda: every(20, cpuTemp)).start()
threading.Thread(target=lambda: every(20, diskUsed)).start()
threading.Thread(target=lambda: every(20, ramFree)).start()
