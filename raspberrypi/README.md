1. Install Raspberry Pi OS

2. Setup the SSH connection with the RPi: put in the boot partition of the SD card an empty file named "ssh" and a file "wpa_supplicant.conf" containing this code. Change field country, ssid and psk accordingly to your network.
    
    ctrl_interface=DIR=/var/run/wpa_supplicant GROUP=netdev
    update_config=1
    country=AT

    network={
     ssid="my_network_ssid"
     psk="my_network_password"
    }
    
3. Find RPi IP address

    ping raspberrypi.local

4. Connect through ssh

    ssh -l pi 172.16.6.115

5. Install node.js

    curl -sL https://deb.nodesource.com/setup_15.x | sudo -E bash -
    
    sudo apt install -y nodejs

6. Create a directory for the mesh_bridge application

    mkdir mesh_bridge
    cd mesh_bridge/

7. Install node modules

    npm install noble
    npm install node-aes-cmac
    npm install big-integer
    npm install colors
    npm install @abandonware/bluetooth-hci-socket

8. Change in library file node_modules/noble/lib/hci-socket/hci.js at line 6:
    
    var BluetoothHciSocket = require('bluetooth-hci-socket');
    
into
    
    var BluetoothHciSocket = require('@abandonware/bluetooth-hci-socket');

9. Copy the Javascript files from the repository into the mesh_bridge folder

11. Change in mesh_bridge.js the hex_netkey and hex_appkey according to your mesh network values (network key and application key).

12. Set to on the proxy state of the proxy node throught the nRF Mesh app. Verify if the sensors are publishing. Disconnect the nRF app from the proxy node before continuing.

10. Run the application

    sudo node mesh_bridge.js
