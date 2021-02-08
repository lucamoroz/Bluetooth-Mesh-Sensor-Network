const noble = require('noble');
const utils = require('./utils.js');
const crypto = require('./crypto.js');
const colors = require('colors');

let config;
try {
  config = require('./config');
} catch (e) {
  console.log("Did you set up the keys?");
  process.exit(1);
}

const MESH_SERVICE_UUID = '1828';
const MESH_CHARACTERISTIC_UUID = '2ade';

let segmentation_buffer = null;
let pdu_segmentation_buffer = [];

//------------------------------------------
// Mesh Network Encryption Key Generation
//------------------------------------------

// provision data statically configured
let hex_iv_index = config.hex_iv_index;
let hex_netkey = config.hex_netkey;
let hex_appkey = config.hex_appkey;

hex_encryption_key = ""; // derived from NetKey using k2
hex_privacy_key = "";    // derived from NetKey using k2
hex_nid = "";            // derived from NetKey using k2
hex_network_id = "";     // derived from NetKey using k3

initialise();

function initialise() {
  crypto.init();
  k2_material = crypto.k2(hex_netkey, "00");
  hex_encryption_key = k2_material.encryption_key;
  hex_privacy_key = k2_material.privacy_key;
  hex_nid = k2_material.NID;
  console.log('Network ID: ' + hex_nid);
  network_id = crypto.k3(hex_netkey);
}

//------------------------------
// GAP proxy node discovery
//------------------------------
noble.on('stateChange', state => {
  if (state === 'poweredOn') {
    console.log('Scanning');
    noble.startScanning([MESH_SERVICE_UUID]);
  } else {
    noble.stopScanning();
  }
});

noble.on('discover', peripheral => {
    // connect to the first peripheral that is scanned
    noble.stopScanning();
    const name = peripheral.advertisement.localName;
    console.log(`Connecting to '${name}' ${peripheral.id}`);
    connectAndSetUp(peripheral);
});

function connectAndSetUp(peripheral) {

  peripheral.connect(error => {
    console.log('Connected to', peripheral.id);

    // specify the services and characteristics to discover
    const serviceUUIDs = [MESH_SERVICE_UUID];
    const characteristicUUIDs = [MESH_CHARACTERISTIC_UUID];

    peripheral.discoverSomeServicesAndCharacteristics(
        serviceUUIDs,
        characteristicUUIDs,
        onServicesAndCharacteristicsDiscovered
    );
  });

  peripheral.on('disconnect', () => {
    console.log('Disconnected. Restarting scan...');
    noble.startScanning([MESH_SERVICE_UUID]);}
  );
}

//---------------------------------
// GATT notifications management
//---------------------------------
function onServicesAndCharacteristicsDiscovered(error, services, characteristics) {
  console.log('Discovered services and characteristics');
  const meshCharacteristic = characteristics[0];
  console.log('Services: ' + services);
  console.log('Characteristics: ' + characteristics);

  // data callback receives notifications
  meshCharacteristic.on('data', (data, isNotification) => {
    var octets = Uint8Array.from(data);
    console.log('Received: "' + utils.u8AToHexString(octets).toUpperCase() + '"');
    logAndValidatePdu(octets);
  });

  // subscribe to be notified whenever the peripheral update the characteristic
  meshCharacteristic.subscribe(error => {
    if (error) {
      console.error('Error subscribing to mesh_proxy_data_out');
    } else {
      console.log('Subscribed for mesh_proxy_data_out notifications');
    }
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
  console.log("sar_msgtype="+sar_msgtype);

  // PDU segmentation
  sar = (sar_msgtype & 0xC0) >> 6;
  if (sar < 0 || sar > 3) {
    console.log(colors.red("SAR contains invalid value. 0-3 allowed. Ref Table 6.2"));
    return;
  } else if (sar == 0) {
    console.log("Complete message: " + utils)
  } else if (sar == 1) {
    segmentation_buffer = null;
    segmentation_buffer = Buffer.from(octets);
    console.log("First segment of message saved");
    return;
  } else if (sar == 2) {
    concatenate(octets.subarray(1, octets.length));
    console.log("Continuation of a message saved");
    return;
  } else if (sar == 3){
    concatenate(octets.subarray(1, octets.length));
    console.log("Last segment of a message saved");
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

  console.log("Proxy PDU: SAR=" + utils.intToHex(sar) + " MSGTYPE=" + utils.intToHex(msgtype) + " NETWORK PDU=" + utils.u8AToHexString(network_pdu));

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

  console.log("enc_dst=" + hex_enc_dst);
  console.log("enc_transport_pdu=" + hex_enc_transport_pdu);
  console.log("NetMIC=" + hex_netmic);

  // -----------------------------------------------------
  // 2. Deobfuscate network PDU - ref 3.8.7.3
  // -----------------------------------------------------
  hex_privacy_random = crypto.privacyRandom(hex_enc_dst, hex_enc_transport_pdu, hex_netmic);
  console.log("Privacy Random=" + hex_privacy_random);

  deobfuscated = crypto.deobfuscate(hex_obfuscated_ctl_ttl_seq_src, hex_iv_index, hex_netkey, hex_privacy_random, hex_privacy_key);
  hex_ctl_ttl_seq_src = deobfuscated.ctl_ttl_seq_src;

  // 3.4.6.3 Receiving a Network PDU
  // Upon receiving a message, the node shall check if the value of the NID field value matches one or more known NIDs
  if (hex_pdu_nid != hex_nid) {
    console.log(colors.red("unknown nid - discarding"));
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
  src_bytes = utils.hexToBytes(hex_pdu_src);
  src_value = src_bytes[0] + (src_bytes[1] << 8);
  if (src_value < 0x0001 || src_value > 0x7FFF) {
    console.log(colors.red("SRC is not a valid unicast address. 0x0001-0x7FFF allowed. Ref 3.4.2.2"));
    return;
  }

  ctl_int = (parseInt(hex_pdu_ctl_ttl, 16) & 0x80) >> 7;
  ttl_int = parseInt(hex_pdu_ctl_ttl, 16) & 0x7F;

  console.log("hex_enc_dst=" + hex_enc_dst);
  console.log("hex_enc_transport_pdu=" + hex_enc_transport_pdu);
  console.log("hex_netmic=" + hex_netmic);

  hex_enc_network_data = hex_enc_dst + hex_enc_transport_pdu + hex_netmic;
  console.log("decrypting and verifying network layer: " + hex_enc_network_data + " key: " + hex_encryption_key + " nonce: " + hex_nonce);
  result = crypto.decryptAndVerify(hex_encryption_key, hex_enc_network_data, hex_nonce, 4);
  console.log("result=" + JSON.stringify(result));
  if (result.status == -1) {
    console.log("ERROR: "+result.error.message);
    return;
  }

  hex_pdu_dst = result.hex_decrypted.substring(0, 4);
  lower_transport_pdu = result.hex_decrypted.substring(4, result.hex_decrypted.length);
  console.log("lower_transport_pdu=" + lower_transport_pdu);

  // lower transport layer: 3.5.2.1
  hex_pdu_seg_akf_aid = lower_transport_pdu.substring(0, 2);
  console.log("hex_pdu_seg_akf_aid=" + hex_pdu_seg_akf_aid);
  seg_int = (parseInt(hex_pdu_seg_akf_aid, 16) & 0x80) >> 7;
  akf_int = (parseInt(hex_pdu_seg_akf_aid, 16) & 0x40) >> 6;
  aid_int = parseInt(hex_pdu_seg_akf_aid, 16) & 0x3F;

  let transmic_len;
  let aszmic;
  let lower_seq_auth;

  if (seg_int == 0) {
    pdu_segmentation_buffer = [lower_transport_pdu.substring(2, lower_transport_pdu.length)];
    aszmic = "00";
    transmic_len = 8; // 32 bits
    lower_seq_auth = hex_pdu_seq;
  } else { // 3.5.2.2 Segmented Access message
    let hdr = parseInt(lower_transport_pdu.substring(2, 8), 16);

    let idx = (hdr & 0x3E0) >> 5; // pick 5 bits
    let tot = hdr & 0x1F; // pick last 5 bits
    let szmic = (hdr >> 23) & 0x1;
    transmic_len = szmic == 0 ? 8 : 16; // 32 or 64 bits
    aszmic = szmic == 0 ? "00" : "80";

    console.log("assembling message " + idx + " of " + tot);

    pdu_segmentation_buffer[idx] = lower_transport_pdu.substring(8, lower_transport_pdu.length);

    // Here the magic happens. We assemble the seq_auth according to 3.5.3.1
    lower_seq_auth = (
      ((parseInt(hex_pdu_seq.substring(0, 6), 16) >> 14) << 14) +
      ((hdr >> 10) & 0x1FFF)
    ).toString(16).padStart(6, "0");

    console.log("lower_seq_auth=" + lower_seq_auth);

    if (tot !== idx) {
      return;
    }
  }

  // upper transport: 3.6.2
  hex_enc_access_payload_transmic = pdu_segmentation_buffer.join("");
  pdu_segmentation_buffer = [];
  console.log("enc_access_transmic=" + hex_enc_access_payload_transmic);

  hex_enc_access_payload = hex_enc_access_payload_transmic.substring(
    0,
    hex_enc_access_payload_transmic.length - transmic_len
  );

  hex_transmic = hex_enc_access_payload_transmic.substring(
    hex_enc_access_payload_transmic.length - transmic_len,
    hex_enc_access_payload_transmic.length
  );

  console.log("enc_access_payload=" + hex_enc_access_payload);
  console.log("transmic=" + hex_transmic);

  // access payload: 3.7.3
  // derive Application Nonce (3.8.5.2)
  hex_app_nonce = "01" + aszmic + lower_seq_auth + hex_pdu_src + hex_pdu_dst + hex_iv_index;
  console.log("application nonce=" + hex_app_nonce);

  console.log("decrypting and verifying access layer: " + hex_enc_access_payload + hex_transmic + " key: " + hex_appkey + " nonce: " + hex_app_nonce);
  result = crypto.decryptAndVerify(
    hex_appkey,
    hex_enc_access_payload + hex_transmic,
    hex_app_nonce,
    transmic_len/2
  );

  console.log("result=" + JSON.stringify(result));
  if (result.status == -1) {
    console.log("ERROR: "+result.error.message);
    return;
  }

  console.log("access payload=" + result.hex_decrypted);

  hex_opcode_and_params = utils.getOpcodeAndParams(result.hex_decrypted);
  console.log("hex_opcode_and_params=" + JSON.stringify(hex_opcode_and_params));
  hex_opcode = hex_opcode_and_params.opcode;
  hex_params = hex_opcode_and_params.params;
  hex_company_code = hex_opcode_and_params.company_code

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

}

// append a Uint8Array to the segmentation buffer
function concatenate(octets) {
  if (segmentation_buffer == null){
    console.log("concatenation error");
    return
  }
  var continuation = Buffer.from(octets);
  var array = [segmentation_buffer,continuation];
  segmentation_buffer = Buffer.concat(array);
  console.log('New Concatenation');
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
  // TODO verify the correctness of the statically configured network key
  console.log("Network ID from beacon: " + utils.u8AToHexString(octets.subarray(2,10)));

  // retrieve IV index
  hex_iv_index = utils.u8AToHexString(octets.subarray(10,14));
  console.log("IV Index: " + hex_iv_index);
  return;
}
