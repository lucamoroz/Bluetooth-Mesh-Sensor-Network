var asmCrypto = require('./asmcrypto.all');
var aesCmac = require('node-aes-cmac').aesCmac;
var utils = require('./utils.js');
var bigInt = require("big-integer");

var ZERO;
var k2_salt;
var k3_salt;
var k4_salt;
var id64_hex;
var id6_hex;

function getAesCmac(hex_key, hex_message) {
	// aesCmac function requires Buffer objects
	b_key = Buffer.from(hex_key, 'hex');
	b_message = Buffer.from(hex_message, 'hex');
	return aesCmac(b_key, b_message);
}

function init() {
	console.log("initialising crypto variables");
	ZERO = '00000000000000000000000000000000';
	k2_salt = s1("736d6b32"); // "smk2"
	k3_salt = s1("736d6b33"); // "smk3"
	k4_salt = s1("736d6b34"); // "smk4"
	id64_hex = utils.bytesToHex(utils.toAsciiCodes('id64'));
	id6_hex = utils.bytesToHex(utils.toAsciiCodes('id6'));
}

/*
  Core Specification
	2.2.1 Security function e
	Security function e generates 128-bit encryptedData from a 128-bit key and
	128-bit plaintextData using the AES-128-bit block cypher as defined in
	FIPS-1971:

		encryptedData = e(key, plaintextData)

	The most significant octet of key corresponds to key[0], the most significant
  octet of plaintextData corresponds to in[0] and the most significant octet of
  encryptedData corresponds to out[0] using the notation specified in FIPS-1971	

	hex in, hex out
	*/

function e(hex_plaintext, hex_key) {
	console.log("AEC-ECB(" + hex_plaintext + "," + hex_key + ")");

	var hex_padding = "";
	var ecb_encrypted = asmCrypto.AES_ECB.encrypt(asmCrypto.hex_to_bytes(hex_plaintext), asmCrypto.hex_to_bytes(hex_key), asmCrypto.hex_to_bytes(hex_padding));

	return asmCrypto.bytes_to_hex(ecb_encrypted);
};

function s1(M) {
	cmac = getAesCmac(ZERO, M);
	// result is a hex encoded string
	return cmac;
}

function k2(N, P) {
	// return a key_material object

	// T = AES-CMACsalt (N)
	// NB the parse function converts a byte array into a CryptoJS WordArray
	T = getAesCmac(k2_salt, N);

	//		T0 = empty string (zero length) 
	T0 = "";

	//		T1 = AES-CMACt (T0 || P || 0x01) 
	M1 = T0 + P.toString() + "01";
	T1 = getAesCmac(T, M1);

	//		T2 = AES-CMACt (T1 || P || 0x02) 
	M2 = T1 + P.toString() + "02";
	T2 = getAesCmac(T, M2);

	//		T3 = AES-CMACt (T2 || P || 0x03)
	M3 = T2 + P.toString() + "03";
	T3 = getAesCmac(T, M3);

	//		k2(N, P) = (T1 || T2 || T3) mod 2(263)
	T123 = T1 + T2 + T3;

	// see https://github.com/peterolson/BigInteger.js
	var TWO_POW_263 = bigInt(2).pow(263);
	var T123_bigint = bigInt(T123, 16);

	modval = T123_bigint.divmod(TWO_POW_263);

	modval_bigint = bigInt(modval.remainder);

	k2_hex = utils.bigIntegerToHexString(modval_bigint);

	var k2_material = {
		NID: 0,
		encryption_key: 0,
		privacy_key: 0
	};

	k2_material.NID = k2_hex.substring(0, 2);
	k2_material.encryption_key = k2_hex.substring(2, 34);
	k2_material.privacy_key = k2_hex.substring(34, 66);

	return k2_material;
};

function k3(N) {

	// T = AES-CMACsalt (N)
	T = getAesCmac(k3_salt.toString(), N);

	//		k3(N) = AES-CMACt ( “id64” || 0x01 ) mod 2^64
	k3_cmac = getAesCmac(T.toString(), id64_hex + "01");

	var k3_cmac_bigint = bigInt(k3_cmac.toString(), 16);
	var TWO_POW_64 = bigInt(2).pow(64);
	k3_modval = k3_cmac_bigint.divmod(TWO_POW_64);
	k3_modval_bigint = bigInt(k3_modval.remainder);

	k3_material = utils.bigIntegerToHexString(k3_modval_bigint);

	return k3_material;
};

// Privacy Random = (EncDST || EncTransportPDU || NetMIC)[0–7]
function privacyRandom(enc_dst, enc_transport_pdu, netmic) {
	console.log("privacyRandom(" + enc_dst + "," + enc_transport_pdu + "," + netmic + ")");
	temp = enc_dst + enc_transport_pdu + netmic;
	if (temp.length >= 14) {
		return temp.substring(0, 14);
	} else {
		return "";
	}
}

function decryptAndVerify(hex_key, hex_cipher, hex_nonce) {
	dec_ver_result = {
		hex_decrypted: "",
		status: 0,
		error: ""
	}
	try {
		var adata = new Uint8Array([ ]);
		dec = asmCrypto.AES_CCM.decrypt( utils.hexToU8A(hex_cipher), utils.hexToU8A(hex_key), utils.hexToU8A(hex_nonce), adata, 4 );
		hex_dec = utils.u8AToHexString(dec);
		dec_ver_result.hex_decrypted = hex_dec;
	} catch (err) {
		dec_ver_result.hex_decrypted = "";
		dec_ver_result.status = -1;
		dec_ver_result.error = err;
	}
	return dec_ver_result;
}

function deobfuscate(obfuscated_data, iv_index, netkey, privacy_random, privacy_key) {
	console.log("deobfuscate");

	var result = {
		privacy_key: '',
		privacy_random: '',
		pecb_input: '',
		pecb: '',
		ctl_ttl_seq_src: '',
	};

	result.privacy_key = privacy_key;
	result.privacy_random = privacy_random;

	// derive PECB
	result.pecb_input = "0000000000" + iv_index + privacy_random;
	pecb_hex = e(result.pecb_input, privacy_key);
	pecb = pecb_hex.substring(0, 12);
	result.pecb = pecb;

	// DeobfuscatedData = ObfuscatedData ⊕ PECB[0–5]

	deobf = utils.xorU8Array(utils.hexToU8A(obfuscated_data), utils.hexToU8A(pecb));
	result.ctl_ttl_seq_src = utils.u8AToHexString(deobf);
	return result;
}

module.exports.getAesCmac = getAesCmac;
module.exports.k2 = k2;
module.exports.k3 = k3;
module.exports.init = init;
module.exports.privacyRandom = privacyRandom;
module.exports.deobfuscate = deobfuscate;
module.exports.decryptAndVerify = decryptAndVerify;
