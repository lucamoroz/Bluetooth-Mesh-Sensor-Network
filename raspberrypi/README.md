# Setup

1. Install Raspberry Pi OS

5. Install node.js

        curl -sL https://deb.nodesource.com/setup_15.x | sudo -E bash     
        sudo apt install -y nodejs

6. Clone the repo and cd into the `mesh_bridge` folder

        git clone git@github.com:lucamoroz/IoTProject.git
        cd IoTProject/raspberrypi/mesh_bridge/

7. Install node modules with npm

        npm i

8. Re-link the abandonware modules to the installed version:

        cd node_modules/
        ln -s "@abandonware/bluetooth-hci-socket" bluetooth-hci-socket
        
9. Set up `config.js` from `config.example.js` according to your network values:
        * `hex_netkey`, mesh network key; 
        * `hex_appkey`, mesh application key;
        * `mqtt_url`, Thingsboard MQTT instance address;
        * `mqtt_token`, authentication token for MQTT;
        * `proxy_ids`, proxy node Bluetooth identifier (it appears while scanning for nodes with nRF Mesh app;
        * `address_map`, mapping of mesh sensor addresses to human readable names

10. Make sure the nodes are not connected to the nRF app before continuing.

11. Run the application

        sudo node mesh_bridge.js
