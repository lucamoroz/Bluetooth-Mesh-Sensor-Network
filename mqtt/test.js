var mqtt = require('mqtt')
var client  = mqtt.connect(
  'mqtts://iot.wussler.it',
  {
    'username':'w7SohHdkRf2ZVvKv' // Token for customer@iot.wussler.it
  }
)

function random(low, high) {
  return Math.random() * (high - low) + low
}

function sendData() {
  let data = JSON.stringify({"temperature": random(20, 30)})
  client.publish('v1/devices/me/telemetry', data)
  console.log("Pubishing: " + data)
}

client.on('connect', function () {
  setInterval(sendData, 5000)
})
