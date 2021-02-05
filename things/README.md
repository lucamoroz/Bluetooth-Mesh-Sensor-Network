# Setup
1. Install VSCode
2. Install Platformio IDE extension: https://platformio.org/install/ide?install=vscode 

# Build
Open the project (e.g. proxy) from PIO Home and press `ctrl+alt+b`

Alternatively, from the project rood dir run `pio run --environment thingy_52`

# Upload
Open the project (e.g. proxy) from PIO Home and press `ctrl+alt+u`

Alternatively, from the project rood dir run `pio run --target upload --environment thingy_52`

# Erase
To reset the devices (e.g. to perform provisioning again), run: `nrfjprog -e`

# Debug
## Serial messages
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