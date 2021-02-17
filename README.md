# Contributors

- Thingsboard: Aron Wussler
- RPi gateway:
  - Bluetooth Mesh packets decryption: Aron Wussler, Luca Tasso
  - Bluetooth Mesh generic onoff sending: Luca Tasso
  - MQTT data out: Aron Wussler
  - MQTT data in (Thingsboard RPCs): Luca Moroldo 
- Things:
  - Proxy & Sensor nodes: Luca Moroldo
  - Sensor values reading: Aron Wussler

# Description
TUWien - IoT project.

Group: Aron Wussler, Luca Moroldo, Luca Tasso

# Requirements
Our plan is to build a system capable of monitoring temperature and air quality of one or more rooms and show this dashboard.
Moreover the sensors will notify any critical air condition through their LED.

The system will be:
- Secure and private: the MQTT connection will be secured (e.g. with password), the bluetooth mesh uses a network and application key, and the bluetooth LE communication will be enstabilished by means of a PIN.
- Asynchronous: derives by the usage of MQTT
- Energy efficient (especially battery-powered devices)


## Nordic Thingy 52
The Nordic Thingy 52 will run Zephyr OS in order to create a Bluetooth Mesh network.

The devices will implement the following models:
- Generic OnOff model to control the onboard LED.
- Sensor model to communicate temperature, humidity, ... .
- Light HSL model to change the color of the LED.

At least one of these devices will use the Bluetooth LE functionality in order to communicate with the Raspberry Pi 3 via GATT.

## Raspberry Pi 3

The RPi3 will run Rasperry Pi OS (previously called Raspbian) with the BlueZ Bluetooth stack in order to communicate with the Bluetooth Mesh network using gattlib.

The communication will occur between the RPi3 and a node of the Bluetooth Mesh network implementing the Bluetooth Mesh Proxy functionality.

Finally, the RPi3 will communicate with the dashboard using MQTT, in order to send and receive data (i.e. it will act both as publisher and subscriber). 

## Dashboard
The dashboard will be ThingsBoard, running as a docker service. 

The dashboard will allow the users to:
- Visualize the sensors data
- Send an alert to turn on or off the sensors LEDs (to simulate a ventilation system activation)

The communication with ThingsBoard will be MQTT-based, that is asynchronous.
