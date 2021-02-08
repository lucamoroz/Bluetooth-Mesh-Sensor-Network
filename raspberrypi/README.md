# Setup

1. Install Raspberry Pi OS
2. Setup the SSH connection with the RPi: put in the boot partition of the SD card an empty file named "ssh" and a file `wpa_supplicant.conf` containing this code. Change field country, ssid and psk accordingly to your network.

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

        ssh -l pi 172.16.6.1157

5. Install node.js

        curl -sL https://deb.nodesource.com/setup_15.x | sudo -E bash     
        sudo apt install -y nodejs

6. Clone the repo and cd into the `mesh_bridge` folder

        git clone ...
        cd mesh_bridge/

7. Install node modules with npm

        npm i

8. Re-link the abandonware modules to the installed version:

        cd node_modules/
        ln -s "@abandonware/bluetooth-hci-socket" bluetooth-hci-socket

9. Set up `config.js` from `config.example.js` the `hex_netkey` and `hex_appkey` according to your
mesh network values (network key and application key).

10. Set to on the proxy state of the proxy node through the nRF Mesh app.
Verify if the sensors are publishing.
Disconnect the nRF app from the proxy node before continuing.

11. Run the application

        sudo node mesh_bridge.js
