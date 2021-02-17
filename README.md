# Contributors

- Thingsboard: Aron Wussler
- RPi gateway:
  - Bluetooth Mesh packets decryption: Aron Wussler, Luca Tasso
  - Bluetooth Mesh generic onoff sending: Luca Tasso
  - MQTT data out: Aron Wussler
  - MQTT data in (Thingsboard RPCs): Luca Moroldo 
- Things:
  - Thingy 52 Proxy & Sensor nodes (provisioning, setup, mesh models, user interaction): Luca Moroldo
  - Sensor values reading: Aron Wussler

# Description
TUWien - IoT project WS 2020/2021.

Group: Aron Wussler, Luca Moroldo, Luca Tasso

# Demo
The ThingsBoard dashboards can be seen on https://iot.wussler.it/ logging in with the credentials `tenant@iot.wussler.it`/`X6Rqcfpmch0T6L1R`

This has been set up using nginx's reverse-proxy functionality with Let's Encrypt TLS certificates.
The same certificates have been converted to PKCS12 to use to secure MQTT over TLS directly on port 8883.

In the folder ThingsBoard the exported configuration is available.

# Requirements
Our plan is to build a system capable of monitoring temperature and air quality of one or more rooms and show this dashboard.
Moreover the sensors will notify any critical air condition through their LED.

The system will be:
- Secure and private: the MQTT connection will be secured, the bluetooth mesh uses a network and application key to hide the content of the messages, which will be decrypted by the Raspberry Pi 3.
- Asynchronous: derives by the usage of MQTT
- Energy efficient (especially battery-powered devices)


## Nordic Thingy 52
The Nordic Thingy 52 will run Zephyr OS in order to create a Bluetooth Mesh network.

The devices will implement the following models:
- Generic OnOff model to control the onboard LED.
- Sensor model to communicate temperature, humidity, pressure, and CO2 level.

At least one of these devices will use the Bluetooth LE functionality in order to communicate with the Raspberry Pi 3 via GATT.

## Raspberry Pi 3

The RPi3 will run Rasperry Pi OS (previously called Raspbian) with the BlueZ Bluetooth stack in order to communicate with the Bluetooth Mesh network.

The communication will occur between the RPi3 and a node of the Bluetooth Mesh network implementing the Bluetooth Mesh Proxy functionality.

Finally, the RPi3 will communicate with the dashboard using MQTT, in order to send and receive data (i.e. it will act both as publisher and subscriber). 

## Dashboard
The dashboard will be based on ThingsBoard. 

The dashboard will allow the users to:
- Visualize the sensors data
- Control the LEDs of the Bluetooth Mesh network

The communication with ThingsBoard will be MQTT-based, that is asynchronous.
