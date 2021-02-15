var numeric_chars = ['0', '1', '2', '3', '4', '5', '6', '7', '8', '9', '.'];

var hex_chars = ['0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'A', 'B', 'C', 'D', 'E', 'F'];

function xorU8Array(bytes1, bytes2) {
    // Uint8Arrays in and out
    if (bytes1.length != bytes2.length) {
        memlog.warn("ERROR in xorU8Array: operands must be the same length");
        return new Uint8Array([]);
    }
    xor_bytes = [];
    for (var i = 0; i < bytes1.length; i++) {
        xor_bytes.push(bytes1[i] ^ bytes2[i]);
    }
    return new Uint8Array(xor_bytes);
}

function u8AToHexString(u8a) {
    hex = '';
    for (var i = 0; i < u8a.length; i++) {
        hex_pair = ('0' + u8a[i].toString(16));
        if (hex_pair.length == 3) {
            hex_pair = hex_pair.substring(1, 3);
        }
        hex = hex + hex_pair;
    }
    return hex;
}

function hexStringToArray(hex) {
    a = [];
    i=0;
    while (i < hex.length) {
        hex_pair = hex.substring(i,i+2);
        a.push(parseInt(hex_pair, 16));
        i = i + 2;
    }
    return a;
}

function bigIntegerToHexString(x) {
    var hexString = x.toString(16);
    return hexString;
}

// ref 3.7.3.1
function getOpcodeAndParams(hex_access_payload) {
    result = {
        opcode: "",
        params: "",
        company_code: "",
        status: -1,
        message: "Invalid parameter length"
    };

    if (hex_access_payload.length < 2) {
        return result;
    }

    byte1 = parseInt(hex_access_payload.substring(0, 2), 16);

    if ((byte1 & 0x7F) == 0x7F) {
        result.message = "Opcode value is reserved for future use";
        return result;
    }

    result.status = 0;
    result.message = "OK";

    opcode_len = 1;
    if ((byte1 & 0x80) == 0x80 && (byte1 & 0x40) != 0x40) {
        opcode_len = 2;
    } else if ((byte1 & 0x80) == 0x80 && (byte1 & 0x40) == 0x40) {
        opcode_len = 3;
    }

    opcode_part = hex_access_payload.substring(0, (2 * opcode_len));
    company_code = "";
    if (opcode_len == 3) {
        company_code = opcode_part.substring(2, 6);
        opcode_part = opcode_part.substring(0, 2);
        opcode = parseInt(opcode_part, 16);
        result.company_code = company_code;
        result.opcode = toHex(opcode, 1);
    } else if (opcode_len == 2) {
        opcode = parseInt(opcode_part, 16);
        result.opcode = toHex(opcode, 2);
    } else {
        opcode = parseInt(opcode_part, 16);
        result.opcode = toHex(opcode, 1);
    }

    result.params = hex_access_payload.substring((2 * opcode_len), hex_access_payload.length);

    return result;
}

function toAsciiCodes(text) {
    var bytes = [];
    for (var i = 0; i < text.length; i++) {
        bytes.push(text.charCodeAt(i));
    }
    return bytes;
}

// Convert a byte array to a hex string
function bytesToHex(bytes) {
    for (var hex = [], i = 0; i < bytes.length; i++) {
        hex.push((bytes[i] >>> 4).toString(16));
        hex.push((bytes[i] & 0xF).toString(16));
    }
    return hex.join("");
}

function intToHex(number) {
    hex = "00" + number.toString(16);
    return hex.substring(hex.length - 2);
}

function hexToU8A(hex) {

    if (hex.length % 2 != 0) {
        console.log("ERROR: hex string must be even number in length and contain nothing but hex chars");
        console.log(hex);
        return;
    }

    bytes = [];

    for (var i = 0; i < hex.length; i = i + 2) {
        bytes.push(parseInt(hex.substring(i, i + 2), 16));
    }
    result = new Uint8Array(bytes);
    return result;
}

function toHex(number, octets) {
    hex = ("0" + (Number(number).toString(16))).toUpperCase();
    if (hex.length % 2 == 1) {
        hex = "0" + hex;
    }
    octet_count = hex.length / 2;
    if (octet_count < octets) {
        added_zeroes = octets - octet_count;
        for (var i = 0; i < added_zeroes; i++) {
            hex = "00" + hex;
        }
    } else if (octet_count > octets) {
        hex = hex.substring((octet_count - octets) * 2, hex_chars.length);
    }
    return hex;
}

function bigIntegerToHexString(x) {
    var hexString = x.toString(16);
    return hexString;
}

function hexToBytes(str) {
    var result = [];
    while (str.length >= 2) {
        result.push(parseInt(str.substring(0, 2), 16));
        str = str.substring(2, str.length);
    }
    return result;
}

function normaliseHex(raw) {
    value = raw.replace(/\s/g, '');
    value = value.toUpperCase();
    return value;
}

function leastSignificantBit(number) {
    return number & 1;
}

module.exports.u8AToHexString = u8AToHexString;
module.exports.getOpcodeAndParams = getOpcodeAndParams;
module.exports.toAsciiCodes = toAsciiCodes;
module.exports.bytesToHex = bytesToHex;
module.exports.intToHex = intToHex;
module.exports.xorU8Array = xorU8Array;
module.exports.hexToU8A = hexToU8A;
module.exports.toHex = toHex;
module.exports.bigIntegerToHexString = bigIntegerToHexString;
module.exports.hexStringToArray = hexStringToArray;
module.exports.u8AToHexString = u8AToHexString;
module.exports.hexToBytes = hexToBytes;
module.exports.normaliseHex = normaliseHex;
module.exports.leastSignificantBit = leastSignificantBit;