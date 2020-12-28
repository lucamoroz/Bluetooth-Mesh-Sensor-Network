# Setup
1. Install VSCode and PlatformIO IDE: https://platformio.org/install/ide?install=vscode
2. Install Zephyr (todo check if this is done by PlatformIO IDE automatically): https://docs.zephyrproject.org/latest/getting_started/installation_linux.html#installation-linux 

# Resources
## Bluetooth
### Mesh
- Specs: https://www.bluetooth.com/specifications/mesh-specifications/?utm_campaign=mesh&amp;utm_source=internal&amp;utm_medium=blog&amp;utm_content=bluetooth-mesh-developer-study-guide-v2.0
- Bluetooth mesh networking overview: https://www.bluetooth.com/bluetooth-resources/bluetooth-mesh-networking-an-introduction-for-developers/
- Mesh models overview: https://www.bluetooth.com/bluetooth-resources/bluetooth-mesh-models/
- Bluetooth mesh study guide with code (version 1.14): https://www.bluetooth.com/bluetooth-resources/bluetooth-mesh-developer-study-guide/

Videos:
- Bluetooth mesh + zephyr intro: https://www.youtube.com/watch?v=9RMElr61SQI&ab_channel=TheLinuxFoundation

# Debug
## Serial messages
1. Install JLink RTT: segger.com/products/debug-probes/j-link/technology/about-real-time-transfer/
2. From terminal run: `JLinkRTTViewerExe`
3. Select USB connection, NRF52832_XXAA as target devie, SWD 4000 kHz, auto detection for RTT control block
4. Connect and see printk() messages