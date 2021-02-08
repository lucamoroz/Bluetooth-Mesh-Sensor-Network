var mqtt = require('mqtt')

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
})

module.exports.send_data = send_data;
