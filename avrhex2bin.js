const fs = require("fs");
const readline = require("readline");

const records = [];

const lineProducer = readline.createInterface({
  input: fs.createReadStream("atmega.ino.hex"),
  output: process.stdout,
  terminal: false,
});

lineProducer.on("line", (line) => {
  const record = parseRecord(line);
  records.push(record);
});

lineProducer.on("close", () => {
  const totalRecords = records.length;
  const lastDataRecord = records[totalRecords - 2];
  const requiredBufferSize = lastDataRecord.offset + lastDataRecord.len;

  const fileBuffer = Buffer.alloc(requiredBufferSize);

  records.forEach((record) => {
    if (record.type === 0) {
        calculateChecksumAndWriteToBuffer(record, fileBuffer)
    } else if (record.type === 1) {
        fs.writeFileSync('atmega.bin', fileBuffer)
    } else {
        throw new Error(`Unexpected recordType`)
    }
  });
});

function parseRecord(hexLine) {
  if (hexLine.length < 11) {
    throw new Error(
      `Wrong HEX file format, missing fields! Line from file was: (${hexLine}).`
    );
  }

  if (hexLine.charAt(0) != ":") {
    throw new Error(
      `Wrong HEX file format, does not start with colon! Line from file was: (${hexLine}).`
    );
  }

  const record = {};

  record.len = convertHex(hexLine.substr(1, 2));
  record.offset = convertHex(hexLine.substr(3, 4));
  record.type = convertHex(hexLine.substr(7, 2));

  if (record.type != 0 && record.type != 1) {
    throw new Error(
      `Unexpected recordType: ${record.type} for record: ${hexLine}`
    );
  }

  if (hexLine.length < 11 + record.len * 2) {
    throw new Error(`Wrong HEX file format, missing fields! ${hexLine}`);
  }

  record.data = hexLine.substr(9);

  return record;
}

function convertHex(hexStr) {
  let result = 0;
  for (var i = 0; i < hexStr.length; i++) {
    result = result << 4;
    result = result | charHexToNum(hexStr.charAt(i));
  }
  return result;
}

function charHexToNum(hexChar) {
  switch (hexChar) {
    case "0":
      return 0;
    case "1":
      return 1;
    case "2":
      return 2;
    case "3":
      return 3;
    case "4":
      return 4;
    case "5":
      return 5;
    case "6":
      return 6;
    case "7":
      return 7;
    case "8":
      return 8;
    case "9":
      return 9;
    case "a":
    case "A":
      return 10;
    case "b":
    case "B":
      return 11;
    case "c":
    case "C":
      return 12;
    case "d":
    case "D":
      return 13;
    case "e":
    case "E":
      return 14;
    case "f":
    case "F":
      return 15;
  }
}

function calculateChecksumAndWriteToBuffer(record, buffer) {
  
  let checksum = record.len;

  checksum += (record.offset >> 8) & 0xff;

  checksum += record.offset & 0xff;

  checksum += record.type;

  let i = 0;
  for (; i < record.len; i++) {
    const hexByte = convertHex(record.data.substr(i * 2, 2));

    checksum += hexByte;

    buffer && buffer.writeUInt8(hexByte, record.offset + i);
  }
  const checksumLsb = checksum & 0xff;

  const dataChecksum = convertHex(record.data.substr(i * 2));

  if (((checksumLsb + dataChecksum) & 0xFF) !== 0) {
    throw new Error(`Checksum doesn't match`);
  }
}

function testRecordParseAndChecksumCalculation() {
    const record = parseRecord(":0300300002337A1E");
    calculateChecksumAndWriteToBuffer(record);
}
