# How to use
## Setup
1. Install VSCode
2. Install Platformio IDE extension: https://platformio.org/install/ide?install=vscode 

## Build
Open the project (e.g. proxy) from PIO Home and press `ctrl+alt+b`

Alternatively, from the project rood dir run `pio run --environment thingy_52`

## Upload
Open the project (e.g. proxy) from PIO Home and press `ctrl+alt+u`

Alternatively, from the project rood dir run `pio run --target upload --environment thingy_52`

## Provisioning
1. Download the application nRF Mesh
2. Press ADD NODE and click on target
3. Press IDENTIFY and then PROVISION
4. Select either No OOB or Outpub OOB, the latter requires you to see the output logs (see Debug section) in order to input the code displayed with the output
5. After successfully configuring the node, disconnect the nRF mesh application.
6. Press the sensor node button for 5 seconds to automatically configure them. The leds will blink green twice after releasing the button.
7. Do the same with the proxy node.

Alternatively you can fully configure your mesh using only the nRF Mesh application.

## Identify sensor node
After being provisioned, a sensor node has an address that identifies it in the mesh network.

This address will be used to associate a sensor measure to a sensor node. 

The address is displayed by pressing the button of a sensor node, which will blink as many times as the node's id.

## Reset a node
If you need to remove a node from the mesh or re-configure it, you can press the node's button for 10 seconds: it will blink red twice and reset itself to the unprovisioned state.

## Erase a device
To fully reset a device, connect it with a JLink probe and run: `nrfjprog -e`.

## Test configuration
To check if messages are sent successfull:
1. Breath on the Sensor node to raise the CO2 level and trigger the sensor. The Sensor light should turn red until the CO2 level goes back to normal, it also sends a message (if correctly configured) that is received by the Proxy node, which will show a green light to notify the user that a Sensor node detected a high level of CO2.
2. Press the Proxy node button to send Bluetooth Mesh messages, which will turn on/off the Sensor lights and send sensor_get requests. You will see debug messages on the RTT console.

## Debug
### Serial messages
1. Install Segger JLink RTT: segger.com/products/debug-probes/j-link/technology/about-real-time-transfer/
2. Run: `JLinkRTTViewerExe`
3. Select USB connection, NRF52832_XXAA as target device, SWD 4000 kHz, auto detection for RTT control block
4. Connect and see printk() messages

# Resources

## PlatformIO 
- PlatformIO+Zephyr: https://docs.platformio.org/en/latest/frameworks/zephyr.html#tutorials
## Zephyr
- Intro video: https://www.youtube.com/watch?v=jR5E5Kz9A-k&t=3s&ab_channel=NordicSemiconductor
- Samples: https://docs.zephyrproject.org/latest/samples/index.html
- Application development: https://docs.zephyrproject.org/latest/application/index.html
## Bluetooth
### Mesh
- Specs: https://www.bluetooth.com/specifications/mesh-specifications/?utm_campaign=mesh&amp;utm_source=internal&amp;utm_medium=blog&amp;utm_content=bluetooth-mesh-developer-study-guide-v2.0
- Bluetooth mesh networking overview: https://www.bluetooth.com/bluetooth-resources/bluetooth-mesh-networking-an-introduction-for-developers/
- Mesh models overview: https://www.bluetooth.com/bluetooth-resources/bluetooth-mesh-models/
- Bluetooth mesh study guide with code (version 1.14): https://www.bluetooth.com/bluetooth-resources/bluetooth-mesh-developer-study-guide/

Videos:
- Bluetooth mesh + zephyr intro: https://www.youtube.com/watch?v=9RMElr61SQI&ab_channel=TheLinuxFoundation