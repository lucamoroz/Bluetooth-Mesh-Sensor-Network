//-------------------
// Provision data
//-------------------

// Network key
exports.hex_netkey = "644B6AD060C9088ADE0F54EF68930F16";
// Application key
exports.hex_appkey = "915C42B6C4D10AE7CA224776D57D249F";
// IV index
exports.hex_iv_index = "12345677";

// RPi mesh network address
exports.hex_rpi_addr = "7FFF";
// Destination mesh address for LED alerts; default: "FFFF", send to all nodes
exports.hex_LED_alert_target = "FFFF";

// MQTT publishing data
exports.mqtt_url = "mqtts://iot.wussler.it";
exports.mqtt_token = "w7SohHdkRf2ZVvKv";

// Proxy node IDs
exports.proxy_ids = ['d1e174cea07c'];

// Mapping mesh addresses to names
// Format: <mesh address>: <human readable name>
exports.address_map = {
  '0003': 'living_room'
}
