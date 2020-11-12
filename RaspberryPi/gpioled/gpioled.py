#! /usr/bin/env python3

import sys
import os
import time
import paho.mqtt.client as mqtt #Python paho mqtt client. For more info https://www.eclipse.org/paho/clients/python/docs/
import RPi.GPIO as GPIO #Library to control the Raspberry Pi GPIO pins

#Wait for the network to available. This is required if you add this script to rc.local
time.sleep(30)


# ======================================================================================================================================
#                                           Modify parameters below for your MQTT broker
# ======================================================================================================================================

topTopic = "/myhome/"
mqttServerAddress = "192.168.255.10"
mqttServerPort = 1883
mqttClientId = 'pihealthscript' #ensure the client id is unique
mqttUsername = "user"
mqttPassword = "password"
mqttUserPass = dict(username=mqttUsername, password=mqttPassword)

#======================================================================================================================================


class Node:
  def __init__(self, topic, pin):
    self.topic = topic
    self.pin = pin

#GPIO NODES
nodes = []
nodes.append( Node("raspberrypi_gpio8", 8) ) #red led connected to gpio 8 using topic "raspberrypi_gpio8"
nodes.append( Node("raspberrypi_gpio12", 12) ) #yellow led connected to gpio 8 using topic "raspberrypi_gpio12"

#Uncomment below if you require additional nodes 
#nodes.append( Node("gpio18", 18) ) #green
#nodes.append( Node("gpio16", 16) ) #blue


#CONFIGURE GPIO
GPIO.setwarnings(False)
GPIO.setmode(GPIO.BOARD) #USING THE PHYSICAL PIN NUMBERING I.E. PIN 0 = PHYSICAL PIN 0 PIN 40 IS PHYSICAL PIN 40

for node in nodes:
  GPIO.setup(node.pin,GPIO.OUT) #Configure all the nodes as OUTPUT

# The callback when the client connects
def on_connect(client, userdata, flags, rc):
  if rc == 0:
    # Subscribing in on_connect() means that if we lose the connection and
    # reconnect then subscriptions will be renewed.

    #subscribe to the restart topic
    device = os.popen("hostname").readline().replace('\r', '').replace('\n', '')
    restartTopic = topTopic + device + "/restart"
    client.subscribe(restartTopic)
 
    #subscribing to the topics for all the nodes, sending the status of the nodes     
    for node in nodes:
      client.subscribe(topTopic + node.topic + "/cmnd/#")
      send_status(node)
    
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
  # print("MSGREC: " + msg.topic + " " + str(msg.payload))
  topicparts = msg.topic.split("/") #Split the message topic
  if len(topicparts) == 5:
    for node in nodes:
      if topicparts[2] == node.topic:
        if topicparts[3].lower() == "cmnd":
          setGpio( node, msg.payload.decode('utf-8') )
  if len(topicparts) == 4:
    if topicparts[3] == 'restart':
      os.popen('sudo shutdown -r now "Received restart command from espio client. RESTARTING NOW!"')

#Function to set the PIN on or off
def setGpio(node, payload):
  topic = topTopic + node.topic + "/stat/result"
  if payload.lower() == 'on':
    GPIO.output(node.pin,True)
    client.publish(topic, payload='{"power":"on"}', qos=0, retain=False)
  if payload.lower() == 'off':
    GPIO.output(node.pin,False)
    client.publish(topic, payload='{"power":"off"}', qos=0, retain=False)
  if payload.lower() == '?':
    send_status(node)

#Function to send the PIN / LED status i.e. is the led on or off
def send_status(node):
  topic = topTopic + node.topic + "/stat/result"
  gpioState = GPIO.input(node.pin)
  if gpioState == 1:
    payload = '{"power":"on"}'
  else:
    payload = '{"power":"off"}'
  client.publish(topic, payload=payload, qos=0, retain=False)

#client = mqtt.Client(client_id=mqttClientId, clean_session=True, userdata=None, transport="websockets") #use websockets
client = mqtt.Client(client_id=mqttClientId, clean_session=True, userdata=None, transport="tcp")
client.username_pw_set(mqttUsername, mqttPassword)
client.on_connect = on_connect
client.on_message = on_message
client.connect(mqttServerAddress, mqttServerPort, 60)

try:
  client.loop_forever()

except KeyboardInterrupt:
  GPIO.cleanup()
