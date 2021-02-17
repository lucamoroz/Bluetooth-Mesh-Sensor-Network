const noble = require('noble');
const colors = require('colors');
const fs = require('fs');
const crypto = require('./crypto.js');
const mqtt = require('./mqtt.js');
const utils = require('./utils.js');

// load configuration file
let config;
try {
  config = require('./config');
} catch (e) {
  console.log("Did you set up the keys?");
  process.exit(1);
}

const MESH_SERVICE_UUID = '1828';
const MESH_CHARACTERISTIC_IN_UUID = '2add';
const MESH_CHARACTERISTIC_OUT_UUID = '2ade';
let meshCharacteristicIn = "";
let meshCharacteristicOut = "";

// proxy client is connected once it receives the IV index from a Mesh Beacon messagge
let isConnected = false;
let sequence_number = 0;
let send_interval;

let onoff_last_value = 0;
let onoff_id = 0;

let segmentation_buffer = null;
let pdu_segmentation_buffer = [];
let latest_window = Array(0x400).fill(-1); // Window for 1024 elements

//------------------------------------------
// Mesh Network Encryption Key Generation
//------------------------------------------

// provision data statically configured
let hex_iv_index = config.hex_iv_index;
let hex_netkey = config.hex_netkey;
let hex_appkey = config.hex_appkey;
let hex_rpi_addr = config.hex_rpi_addr;
let hex_LED_alert_target = config.hex_LED_alert_target;

hex_encryption_key = ""; // derived from NetKey using k2
hex_privacy_key = "";    // derived from NetKey using k2
hex_nid = "";            // derived from NetKey using k2
hex_aid = "";            // derived from AppKey using k4
hex_network_id = "";     // derived from NetKey using k3

initialise();

function initialise() {
  crypto.init();
  k2_material = crypto.k2(hex_netkey, "00");
  hex_encryption_key = k2_material.encryption_key;
  hex_privacy_key = k2_material.privacy_key;
  hex_nid = k2_material.NID;
  hex_aid = crypto.k4(hex_appkey);
  console.log('Network ID: ' + hex_nid);
  network_id = crypto.k3(hex_netkey);

  // restore sequence number
  fs.readFile('seq', 'utf8', function(err, data){ 
    if (!data) {
        console.log('Creating seq file...');
        fs.writeFile('seq', sequence_number.toString(), (err) => { 
            if (err) throw err; 
        });
    } else {
      sequence_number = parseInt(data);
    }
  });
}

//------------------------------
// GAP proxy node discovery
//------------------------------
noble.on('stateChange', state => {
  if (state === 'poweredOn') {
    console.log('Scanning...');
    noble.startScanning([MESH_SERVICE_UUID]);
  } else {
    noble.stopScanning();
  }
});

noble.on('discover', peripheral => {
  const name = peripheral.advertisement.localName;
  if (!config.proxy_ids.includes(peripheral.id)) {
    console.log(`Found device: "${peripheral.id}" - not whitelisted, skipping`);
    return;
  }

  // connect to the first whitelisted peripheral that is scanned
  noble.stopScanning();
  console.log(`Connecting to device "${peripheral.id}"`);
  connectAndSetUp(peripheral);
});

function connectAndSetUp(peripheral) {

  peripheral.connect(error => {
    console.log(`Connected to "${peripheral.id}"`);

    // specify the services and characteristics to discover
    const serviceUUIDs = [MESH_SERVICE_UUID];
    const characteristicUUIDs = [MESH_CHARACTERISTIC_OUT_UUID,MESH_CHARACTERISTIC_IN_UUID];

    peripheral.discoverSomeServicesAndCharacteristics(
        serviceUUIDs,
        characteristicUUIDs,
        onServicesAndCharacteristicsDiscovered
    );
  });

  peripheral.on('disconnect', () => {
    console.log('Disconnected. Restarting scan...');
    isConnected = false;
    clearInterval(send_interval);
    noble.startScanning([MESH_SERVICE_UUID]);}
  );
}

//---------------------------------
// GATT notifications management
//---------------------------------
function onServicesAndCharacteristicsDiscovered(error, services, characteristics) {


  console.log('Discovered services and characteristics');
  // console.log('Services: ' + services);
  characteristics.forEach(function(characteristic) {
    if (characteristic.uuid == MESH_CHARACTERISTIC_IN_UUID) {
      meshCharacteristicIn = characteristic;
      // console.log('Characteristics mesh_proxy_data_in: ' + meshCharacteristicIn);
    }
    if (characteristic.uuid == MESH_CHARACTERISTIC_OUT_UUID) {
      meshCharacteristicOut = characteristic;
      // console.log('Characteristics mesh_proxy_data_out: ' + meshCharacteristicOut);
    }
  })
  
  // data callback receives notifications
  meshCharacteristicOut.on('data', (data, isNotification) => {
    var octets = Uint8Array.from(data);
    //console.log('Received: "' + utils.u8AToHexString(octets).toUpperCase() + '"');
    logAndValidatePdu(octets);
  });

  // subscribe to be notified whenever the peripheral update the characteristic
  meshCharacteristicOut.subscribe(error => {
    if (error) {
      console.error('Error subscribing to mesh_proxy_data_out');
    } else {
      console.log('Subscribed for mesh_proxy_data_out notifications');
    }
  });

  send_interval = setInterval(send_to_proxy, 500);
}


// periodically check new messages from MQTT and send them to the mesh network
function send_to_proxy(){
  if (!isConnected) {
    console.log('ERROR: IV index has not been configured by Mesh Beacon yet!');
    return;
  }

  if(mqtt.check_new_onoff() == onoff_last_value) {
    // no new messages from MQTT has been received
    return;
  }

  // configure access payload parameters
  let destination = hex_LED_alert_target;
  let onoff_value = utils.toHex(mqtt.check_new_onoff(),1);
  let opcode = '8203';
  let params = `${utils.toHex(onoff_value,1)}${utils.toHex(onoff_id,2)}`;
  onoff_last_value = mqtt.check_new_onoff();
  onoff_id++;
  
  console.log(colors.red.bold(`Sending on/off alert to mesh: ${(onoff_value == 1) ? "ON" : "OFF"}`));
  let segments = build_message(opcode, params, destination);
  segments.forEach(function(segment) {
    // console.log(`Sending segment: ${segment}`);
    let octets = utils.hexToU8A(segment)
    let data = Buffer.from(octets);
    meshCharacteristicIn.write(data, true, error => {
      if (error) {
        console.log('Error sending to mesh_proxy_data_in');
      }
    }); 
  });
} 



//----------------------------------
// Proxy PDU Decryption function
//----------------------------------
function logAndValidatePdu(octets) {

  // length validation
  if (octets.length < 1) {
    console.log(colors.red("Error: No data received"));
    return;
  }

  // -----------------------------------------------------
  // 1. Extract proxy PDU fields : SAR, msgtype, data
  // -----------------------------------------------------
  sar_msgtype = octets.subarray(0, 1);
  // console.log("sar_msgtype="+sar_msgtype);

  // PDU segmentation
  sar = (sar_msgtype & 0xC0) >> 6;
  if (sar < 0 || sar > 3) {
    // console.log(colors.red("SAR contains invalid value. 0-3 allowed. Ref Table 6.2"));
    return;
  } else if (sar == 0) {
    // console.log("Complete message: " + utils.u8AToHexString(octets));
  } else if (sar == 1) {
    segmentation_buffer = null;
    segmentation_buffer = Buffer.from(octets);
    // console.log("First segment of message saved");
    return;
  } else if (sar == 2) {
    concatenate(octets.subarray(1, octets.length));
    // console.log("Continuation of a message saved");
    return;
  } else if (sar == 3){
    concatenate(octets.subarray(1, octets.length));
    // console.log("Last segment of a message saved");
    octets = Uint8Array.from(segmentation_buffer);
  }


  msgtype = sar_msgtype & 0x3F;
  if (msgtype < 0 || msgtype > 3) {
    console.log(colors.red("Message Type contains invalid value. 0x00-0x03 allowed. Ref Table 6.3"));
    return;
  } else if (msgtype == 1) {
    // mesh beacon received
    extract_mesh_beacon(octets.subarray(1, octets.length));
    return;
  }

  // See table 3.7 for min length of network PDU and 6.1 for proxy PDU length
  if (octets.length < 15) {
    console.log(colors.red("PDU is too short (min 15 bytes) - " + octets.length+" bytes received"));
    return;
  }

  network_pdu = null;
  network_pdu = octets.subarray(1, octets.length);

  // console.log("Proxy PDU: SAR=" + utils.intToHex(sar) + " MSGTYPE=" + utils.intToHex(msgtype) + " NETWORK PDU=" + utils.u8AToHexString(network_pdu));

  // demarshall obfuscated network pdu
  pdu_ivi = network_pdu.subarray(0, 1) & 0x80;
  pdu_nid = network_pdu.subarray(0, 1) & 0x7F;
  obfuscated_ctl_ttl_seq_src = network_pdu.subarray(1, 7);
  enc_dst = network_pdu.subarray(7, 9);
  enc_transport_pdu = network_pdu.subarray(9, network_pdu.length - 4);
  netmic = network_pdu.subarray(network_pdu.length - 4, network_pdu.length);

  hex_pdu_ivi = utils.intToHex(pdu_ivi);
  hex_pdu_nid = utils.intToHex(pdu_nid);
  hex_obfuscated_ctl_ttl_seq_src = utils.u8AToHexString(obfuscated_ctl_ttl_seq_src);
  hex_enc_dst = utils.u8AToHexString(enc_dst);
  hex_enc_transport_pdu = utils.u8AToHexString(enc_transport_pdu);
  hex_netmic = utils.intToHex(netmic);

  // console.log("enc_dst=" + hex_enc_dst);
  // console.log("enc_transport_pdu=" + hex_enc_transport_pdu);
  // console.log("NetMIC=" + hex_netmic);

  // -----------------------------------------------------
  // 2. Deobfuscate network PDU - ref 3.8.7.3
  // -----------------------------------------------------
  hex_privacy_random = crypto.privacyRandom(hex_enc_dst, hex_enc_transport_pdu, hex_netmic);
  // console.log("Privacy Random=" + hex_privacy_random);

  deobfuscated = crypto.deobfuscate(hex_obfuscated_ctl_ttl_seq_src, hex_iv_index, hex_netkey, hex_privacy_random, hex_privacy_key);
  hex_ctl_ttl_seq_src = deobfuscated.ctl_ttl_seq_src;

  // 3.4.6.3 Receiving a Network PDU
  // Upon receiving a message, the node shall check if the value of the NID field value matches one or more known NIDs
  if (hex_pdu_nid != hex_nid) {
    console.log(colors.red("ERROR:unknown NID. Discarding message."));
    return;
  }

  hex_enc_dst = utils.bytesToHex(enc_dst);
  hex_enc_transport_pdu = utils.bytesToHex(enc_transport_pdu);
  hex_netmic = utils.bytesToHex(netmic);


  // ref 3.8.7.2 Network layer authentication and encryption

  // -----------------------------------------------------
  // 3. Decrypt and verify network PDU - ref 3.8.5.1
  // -----------------------------------------------------

  hex_nonce = "00" + hex_ctl_ttl_seq_src + "0000" + hex_iv_index;
  hex_pdu_ctl_ttl = hex_ctl_ttl_seq_src.substring(0, 2);

  hex_pdu_seq = hex_ctl_ttl_seq_src.substring(2, 8);
  // NB: SEQ should be unique for each PDU received. We don't enforce this rule here to allow for testing with the same values repeatedly.

  hex_pdu_src = hex_ctl_ttl_seq_src.substring(8, 12);
  // validate SRC
  src_int = parseInt(hex_pdu_src,16);
  if (src_int < 1 || src_int > 32767) {
    console.log(colors.red("SRC is not a valid unicast address. 0x0001-0x7FFF allowed. Ref 3.4.2.2"));
    return;
  }

  ctl_int = (parseInt(hex_pdu_ctl_ttl, 16) & 0x80) >> 7;
  ttl_int = parseInt(hex_pdu_ctl_ttl, 16) & 0x7F;

  // console.log("hex_enc_dst=" + hex_enc_dst);
  // console.log("hex_enc_transport_pdu=" + hex_enc_transport_pdu);
  // console.log("hex_netmic=" + hex_netmic);

  hex_enc_network_data = hex_enc_dst + hex_enc_transport_pdu + hex_netmic;
  // console.log("decrypting and verifying network layer: " + hex_enc_network_data + " key: " + hex_encryption_key + " nonce: " + hex_nonce);
  result = crypto.decryptAndVerify(hex_encryption_key, hex_enc_network_data, hex_nonce, 4);
  // console.log("result=" + JSON.stringify(result));
  if (result.status == -1) {
    console.log("ERROR: "+result.error.message);
    return;
  }

  hex_pdu_dst = result.hex_decrypted.substring(0, 4);
  lower_transport_pdu = result.hex_decrypted.substring(4, result.hex_decrypted.length);
  // console.log("lower_transport_pdu=" + lower_transport_pdu);

  // lower transport layer: 3.5.2.1
  hex_pdu_seg_akf_aid = lower_transport_pdu.substring(0, 2);
  // console.log("hex_pdu_seg_akf_aid=" + hex_pdu_seg_akf_aid);
  seg_int = (parseInt(hex_pdu_seg_akf_aid, 16) & 0x80) >> 7;
  akf_int = (parseInt(hex_pdu_seg_akf_aid, 16) & 0x40) >> 6;
  aid_int = parseInt(hex_pdu_seg_akf_aid, 16) & 0x3F;

  let transmic_len;
  let aszmic;

  if (seg_int == 0) {
    pdu_segmentation_buffer = [lower_transport_pdu.substring(2, lower_transport_pdu.length)];
    aszmic = "00";
    transmic_len = 8; // 32 bits
  } else { // 3.5.2.2 Segmented Access message
    let hdr = parseInt(lower_transport_pdu.substring(2, 8), 16);

    let idx = (hdr & 0x3E0) >> 5; // pick 5 bits
    let tot = hdr & 0x1F; // pick last 5 bits
    let szmic = (hdr >> 23) & 0x1;
    transmic_len = szmic == 0 ? 8 : 16; // 32 or 64 bits
    aszmic = szmic == 0 ? "00" : "80";

    // console.log("assembling message " + idx + " of " + tot);

    pdu_segmentation_buffer[idx] = lower_transport_pdu.substring(8, lower_transport_pdu.length);

    // Here the magic happens. We assemble the seq_auth according to 3.5.3.1
    hex_pdu_seq = (
      (parseInt(hex_pdu_seq.substring(0, 6), 16) & 0xFFE000) + // Upper 11 bits
      ((hdr >> 10) & 0x1FFF) // Lower 13 bits
    ).toString(16).padStart(6, "0");

    if (tot !== idx) {
      return;
    }
  }

  // Check packet duplication
  let seq = parseInt(hex_pdu_seq, 16);
  if (
    latest_window[seq & 0x400] >= seq && // Number in window is in the "past"
    latest_window[seq & 0x400] - seq < 0x1000 // And not too far in the past (prevent loopback)
  ) {
    // console.log("Duplicate packet " + hex_pdu_seq + ", ignoring")
    return;
  }

  // Add to window
  latest_window[seq & 0x400] = seq;

  // upper transport: 3.6.2
  hex_enc_access_payload_transmic = pdu_segmentation_buffer.join("");
  pdu_segmentation_buffer = [];
  // console.log("enc_access_transmic=" + hex_enc_access_payload_transmic);

  hex_enc_access_payload = hex_enc_access_payload_transmic.substring(
    0,
    hex_enc_access_payload_transmic.length - transmic_len
  );

  hex_transmic = hex_enc_access_payload_transmic.substring(
    hex_enc_access_payload_transmic.length - transmic_len,
    hex_enc_access_payload_transmic.length
  );

  // console.log("enc_access_payload=" + hex_enc_access_payload);
  // console.log("transmic=" + hex_transmic);

  // access payload: 3.7.3
  // derive Application Nonce (3.8.5.2)
  hex_app_nonce = "01" + aszmic + hex_pdu_seq + hex_pdu_src + hex_pdu_dst + hex_iv_index;
  // console.log("application nonce=" + hex_app_nonce);

  // console.log("decrypting and verifying access layer: " + hex_enc_access_payload + hex_transmic + " key: " + hex_appkey + " nonce: " + hex_app_nonce);
  result = crypto.decryptAndVerify(
    hex_appkey,
    hex_enc_access_payload + hex_transmic,
    hex_app_nonce,
    transmic_len/2
  );

  // console.log("result=" + JSON.stringify(result));
  if (result.status == -1) {
    console.log("ERROR: "+result.error.message);
    return;
  }

  // console.log("access payload=" + result.hex_decrypted);

  hex_opcode_and_params = utils.getOpcodeAndParams(result.hex_decrypted);
  // console.log("hex_opcode_and_params=" + JSON.stringify(hex_opcode_and_params));
  hex_opcode = hex_opcode_and_params.opcode;
  hex_params = hex_opcode_and_params.params;
  hex_company_code = hex_opcode_and_params.company_code

  /*
  console.log(" ");
  console.log("----------");
  console.log(colors.yellow.bold("Proxy PDU"));
  console.log(colors.green("  SAR=" + utils.intToHex(sar)));
  console.log(colors.green("  MESSAGE TYPE=" + utils.intToHex(msgtype)));
  console.log(colors.yellow.bold("  NETWORK PDU"));
  console.log(colors.green("    IVI=" + hex_pdu_ivi));
  console.log(colors.green("    NID=" + hex_pdu_nid));
  console.log(colors.green("    CTL=" + utils.intToHex(ctl_int)));
  console.log(colors.green("    TTL=" + utils.intToHex(ttl_int)));
  console.log(colors.green("    SEQ=" + hex_pdu_seq));
  console.log(colors.green("    SRC=" + hex_pdu_src));
  console.log(colors.green("    DST=" + hex_pdu_dst));
  console.log(colors.yellow.bold("    Lower Transport PDU"));
  console.log(colors.green("      SEG=" + utils.intToHex(seg_int)));
  console.log(colors.green("      AKF=" + utils.intToHex(akf_int)));
  console.log(colors.green("      AID=" + utils.intToHex(aid_int)));
  console.log(colors.yellow.bold("      Upper Transport PDU"));
  console.log(colors.yellow.bold("        Access Payload"));
  console.log(colors.green("          opcode=" + hex_opcode));
  if (hex_company_code != "") {
    console.log(colors.green("          company_code=" + hex_company_code));
  }
  console.log(colors.green("          params=" + hex_params));
  console.log(colors.green("        TransMIC=" + hex_transmic));
  console.log(colors.green("    NetMIC=" + hex_netmic));
  */

  let decoded = decode_message(hex_pdu_src, hex_params);
  if (decoded.err == "unknown message"){
    return;
  }
  console.log(colors.blue.bold(`New message received from node ${hex_pdu_src}:`));
  console.log(decoded);
  mqtt.send_data(decoded);
}

// append a Uint8Array to the segmentation buffer
function concatenate(octets) {
  if (segmentation_buffer == null){
    console.log("ERROR: proxy PDU concatenation error");
    return
  }
  var continuation = Buffer.from(octets);
  var array = [segmentation_buffer,continuation];
  segmentation_buffer = Buffer.concat(array);
  // console.log('New Concatenation');
  return;
}

// extract and validate Mesh Beacon message
function extract_mesh_beacon(octets) {
  // check if beacon type is correct
  var beacon_type = octets.subarray(0,1);
  if (beacon_type != 1){
    console.log("Error: beacon is not of Secure Network type");
    return;
  }

  // check flags
  // TODO update IV index

  // check NID
  // console.log("Network ID from beacon: " + utils.u8AToHexString(octets.subarray(2,10)));

  // retrieve IV index
  hex_iv_index = utils.u8AToHexString(octets.subarray(10,14));
  // console.log("IV Index: " + hex_iv_index);
  isConnected = true;
  return;
}

function read_short_le(number) {
  return (
    parseInt(number.substring(0, 2), 16) +
    (parseInt(number.substring(2, 4), 16) << 8)
  );
}

function decode_message(sender, message) {
  message = message.toLowerCase();
  let name = get_name(sender);

  switch (message.substring(0, 4)) {
    case "102a":
      return decode_thp(name, message);

    case "132a":
      return decode_gas(name, message);

    default:
      // console.log("Error: unknown message");
      return obj = {err: "unknown message"}
      //return {}
  }
}

function get_name(address) {
  if (address in config.address_map) {
    return config.address_map[address];
  }

  return address;
}

function decode_thp(name, message) {
  if (
    message.substring(0, 4) !== "102a"
    || message.substring(8, 12) !== "112a"
    || message.substring(16, 20) !== "122a"
  ) {
    console.log("Error: malformed thp message");
    return {};
  }

  let obj = {};
  obj['temperature_' + name] = read_short_le(message.substring(4, 8)) / 100;
  obj['humidity_' + name] = read_short_le(message.substring(12, 16)) / 100;
  obj['pressure_' + name] = read_short_le(message.substring(20, 24)) / 100;

  return obj;
}

function decode_gas(name, message) {
  if (message.substring(0, 4) !== "132a") {
    console.log("Error: malformed gas message");
    return {};
  }

  let obj = {};
  obj['co2_ppm_' + name] = read_short_le(message.substr(4, 8));

  return obj;
}

// Assemble new mesh message for sending
function build_message(opcode, params, hex_dst) {
  // console.log("Assembling new mesh message...");

  // access PDU content
  let hex_access_payload = `${opcode}${params}`;
  // console.log(`Access Payload: ${hex_access_payload}`);

  // upper transport PDU content
  // !! nonce works only for unsegmented access PDUs !!
  let hex_pdu_seq = utils.toHex(sequence_number, 3);
  hex_app_nonce = "0100" + hex_pdu_seq + hex_rpi_addr + hex_dst + hex_iv_index;
  let utp_enc_result = crypto.meshAuthEncAccessPayload(hex_appkey, hex_app_nonce, hex_access_payload);
  let upper_trans_pdu = `${utp_enc_result.EncAccessPayload}${utp_enc_result.TransMIC}`;
  // console.log(`Upper Transport PDU: ${upper_trans_pdu}`);

  // check if upper transport PDU is too long and must be segmented
  if (upper_trans_pdu.length > 30) {
      console.log("WARNING: Access payload is too long. Lower Transport Layer segmentation required.")
      return;
  }

  // lower transport PDU content
  let seg_int = parseInt(0, 16);
  let akf_int = parseInt(1, 16);
  let aid_int = parseInt(hex_aid, 16);    
  let seg_afk_aid = (seg_int << 7) | (akf_int << 6) | aid_int;
  let lower_transport_pdu = `${utils.intToHex(seg_afk_aid)}${upper_trans_pdu}`;
  // console.log(`Lower Transport PDU: ${lower_transport_pdu}`);

  // encrypt network PDU
  let ctl_int = parseInt(0, 16);
  let ttl_int = parseInt(2, 16);
  let ctl_ttl = (ctl_int | ttl_int);
  let npdu2 = utils.intToHex(ctl_ttl);
  let norm_enc_key = utils.normaliseHex(hex_encryption_key);
  let hex_net_nonce = "00" + npdu2 + hex_pdu_seq + hex_rpi_addr + "0000" + hex_iv_index;
  let np_enc_result = crypto.meshAuthEncNetwork(norm_enc_key, hex_net_nonce, hex_dst, lower_transport_pdu);
  let enc_dst = np_enc_result.EncDST;
  let enc_transport_pdu = np_enc_result.EncTransportPDU;
  let netmic = np_enc_result.NetMIC;
  // console.log(`Encrypted network payload: ${enc_dst}${enc_transport_pdu}${netmic}`)
  
  // obfuscate network header
  let obfuscated = crypto.obfuscate(enc_dst, enc_transport_pdu,
      netmic, 0, 2, hex_pdu_seq, hex_rpi_addr, hex_iv_index, hex_privacy_key);
  let obfuscated_ctl_ttl_seq_src = obfuscated.obfuscated_ctl_ttl_seq_src;
  // console.log(`Obfuscated network header: ${obfuscated_ctl_ttl_seq_src}`)

  // assemble complete network PDU
  let ivi = utils.leastSignificantBit(parseInt(utils.normaliseHex(hex_iv_index), 16));
  let ivi_int = parseInt(ivi, 16);
  let nid_int = parseInt(hex_nid, 16);
  let npdu1 = utils.intToHex((ivi_int << 7) | nid_int);
  let network_pdu = npdu1 + obfuscated_ctl_ttl_seq_src + enc_dst + enc_transport_pdu + netmic;
  // console.log(`Network PDU: ${network_pdu}`);

  // proxy PDU header
  let proxy_pdu = "";
  let segments = [];

  // console.log(network_pdu.length);
  if (network_pdu.length > 38) {
      // console.log(`Proxy PDU is too long. Segmenting...`);
      segments[0] = proxy_pdu + utils.toHex(64,1) + network_pdu.substring(0,38);
      // console.log(`First Proxy PDU segment: ${segments[0]}`);
      segmented_npdu = network_pdu.substring(38);
      let i = 1;
      while (segmented_npdu.length > 38) {
          segments[i] = proxy_pdu + utils.toHex(80,1) + segmented_npdu.substring(0,38);
          segmented_npdu = segmented_npdu.substring(38);
          i++;
          // console.log(`Intermediate Proxy PDU segment: ${segments[i]}`);
      };
      segments[i] = proxy_pdu + utils.toHex(192,1) + segmented_npdu;
      // console.log(`Last Proxy PDU segment: ${segments[i]}`);
  } else {
      segments[0] = proxy_pdu + utils.toHex(0,1) + network_pdu;
      // console.log(`Proxy PDU: ${segments[0]}`);
  }

  // update sequence number
  sequence_number++;
  fs.writeFile('seq', sequence_number.toString(), (err) => { 
    if (err) throw err; 
  });
  
  return segments;
}
