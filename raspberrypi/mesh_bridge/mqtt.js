const mqtt = require('mqtt')

let config;
try {
  config = require('./config');
} catch (e) {
  console.log("Did you set up the keys?");
  process.exit(1);
}

status_connected = false;
on_or_off = 0;

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
  console.log("Publishing: " + encoded)

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
    on_or_off = params.onoff;
    console.log("Generic onoff message set with value " + on_or_off + " ready for sending.");
  }
});

function check_new_onoff(){
  return on_or_off;
}

module.exports.send_data = send_data;
module.exports.check_new_onoff = check_new_onoff;