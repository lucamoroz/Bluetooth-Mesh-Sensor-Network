const mqtt = require('mqtt')
const mesh_bridge = require('./mesh_bridge.js')

let config;
try {
  config = require('./config');
} catch (e) {
  console.log("Did you set up the keys?");
  process.exit(1);
}

status_connected = false;

var client  = mqtt.connect(
  config.mqtt_url,
  {
    'username':config.mqtt_token
  }
)

function send_data(data) {
  if (!status_connected) {
    console.log("Error: publishing failed, MQTT is disconnected.")
    return false;
  }

  let encoded = JSON.stringify(data)
  client.publish('v1/devices/me/telemetry', encoded)
  console.log("Pubishing: " + encoded)

  return true;
}

client.on('connect', function () {
  console.log("MQTT connected to: " + config.mqtt_url)
  status_connected = true;

  // subscribe to RPCs from the server to this device
  client.subscribe('v1/devices/me/rpc/request/+')
})

client.on('message', function (topic, message) {
  console.log('Received RPC message');
  console.log('request.topic: ' + topic);
  console.log('request.body: ' + message.toString());
  
  var request = JSON.parse(message.toString());

  if (request.method == "onoff-set") {
    var params = JSON.parse(request.params);
    var on_or_off = params.onoff;

    console.log("Sending generic onoff message set with value " + on_or_off);
    mesh_bridge.send_to_proxy(on_or_off);
  }
});

module.exports.send_data = send_data;
