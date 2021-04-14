#include <SPI.h>
#include <FS.h>
#include <LittleFS.h>

#define WORD       2
#define PAGE_SIZE  32 //number of words
#define PAGE_BYTES PAGE_SIZE * WORD //number of bytes

uint8_t pageBuffer[PAGE_BYTES];

void setup() {
  
  LittleFS.begin();
  Serial.begin(115200);
  Serial.println();

  pinMode(D3, OUTPUT);
  digitalWrite(D3, HIGH);

  Serial.println("ESP started");
  Serial.println("Programming start");

  delay(2000);

  serialProgramBegin();
  
  if (programmingEnable()) {
    Serial.println("Proceed...");
    if (!verifyDeviceSignature()) {
      return;
    }
    if (!verifyFuseBits()) {
      Serial.println("Set the correct FUSE");
    }

    chipErase();
    writeFlash();
    verifyFlash();

  } else {
    Serial.println("Stop...");
  }
  serialProgramEnd();
  LittleFS.end();
}

void serialProgramBegin() {
  SPI.begin();
  SPI.setFrequency(150e3);
  SPI.setHwCs(false);
  
  SPI.transfer(0x00);
  digitalWrite(D3, HIGH);
  delayMicroseconds(50);
  digitalWrite(D3, LOW);
  delay(30);
}

bool verifyDeviceSignature() {
  uint8_t manufacturer = spiInstruction(0x30, 0x00, 0x00, 0xFF);
  uint8_t flashSize = spiInstruction(0x30, 0x00, 0x01, 0xFF);
  uint8_t deviceCode = spiInstruction(0x30, 0x00, 0x02, 0xFF);
  Serial.println("Device Signature : " + String(manufacturer, HEX) + " " + String(flashSize, HEX) + " " + String(deviceCode, HEX));
  if (manufacturer == 0x1E //indicates manufactured by Microchip
      && flashSize == 0x93 //indicates 8KB Flash memory
      && deviceCode == 0x07 //indicates ATmega8A device
      ) {
        return true;
      }
  return false;
}

bool verifyFuseBits() {
  uint8_t lowFB = spiInstruction(0x50, 0x00, 0x00, 0xFF);
  Serial.println("Low Fuse Bits: " + String(lowFB, HEX));
  uint8_t highFB = spiInstruction(0x58, 0x08, 0x00, 0xFF);
  Serial.println("High Fuse Bits: " + String(highFB, HEX));
  if (lowFB == 0xE4 && highFB == 0xD9) {
    return true;
  }
  return false;
}

void serialProgramEnd() {
  SPI.end();
  digitalWrite(D3, HIGH);
}

uint8_t spiInstruction(uint8_t a, uint8_t b, uint8_t c, uint8_t d) {
    SPI.transfer(a);
    SPI.transfer(b);
    SPI.transfer(c);
    return SPI.transfer(d);
}

bool programmingEnable() {
    uint8_t firstByte = 0, secondByte = 0, thirdByte = 0, fourthByte = 0;
    firstByte  = SPI.transfer(0xAC);
    secondByte = SPI.transfer(0x53);
    thirdByte  = SPI.transfer(0x00);
    fourthByte = SPI.transfer(0x00);
    Serial.print(firstByte, HEX); Serial.print(secondByte, HEX); Serial.print(thirdByte, HEX); Serial.println(fourthByte, HEX);
    if (thirdByte == 0x53) {
      return true;
    }
    return false;
}

bool chipErase() {
  spiInstruction(0xAC, 0x80, 0x00, 0x00);
  delay(10);
}

bool verifyFlash() {
  File atmegaFile = LittleFS.open("atmega.bin", "r");
  size_t totalNumberOfBytesRead = -1;
  bool match = true;

  while(1) {
    uint8_t bytesReadInBatch = atmegaFile.read(pageBuffer, PAGE_BYTES);
    Serial.print("Number of bytes read "); Serial.println(bytesReadInBatch);
    for(uint8_t i = 0; i < bytesReadInBatch; i++) {
      totalNumberOfBytesRead += 1;
      uint8_t lowByteHighByteInstruction = (i & 0x01) ? 0x28 : 0x20;

      size_t currentWord = (totalNumberOfBytesRead >> 1);
      uint8_t instructionByte2 = (currentWord >> 8 ) & 0x0F;
      uint8_t instructionByte3 = currentWord & 0x00FF;

      uint8_t valueByte = spiInstruction(lowByteHighByteInstruction, instructionByte2, instructionByte3, 0x00);

      Serial.print("Address: "); Serial.print(totalNumberOfBytesRead); Serial.print(" File: "); Serial.print(pageBuffer[i], HEX); Serial.print(" Chip: "); Serial.println(valueByte, HEX);

      if (pageBuffer[i] != valueByte) {
        match = false;
      }
    }
    if (bytesReadInBatch < PAGE_BYTES) {
      break;
    }
  }
  atmegaFile.close();
  return match;
}

void writeFlash() {
  File atmegaFile = LittleFS.open("atmega.bin", "r");
  size_t numberOfBytesRead = 0;
  uint8_t currentPage = 0;

  while(1) {
    numberOfBytesRead = atmegaFile.read(pageBuffer, PAGE_BYTES);
    Serial.print("Number of bytes read"); Serial.println(numberOfBytesRead);
    bool isLowOrHigh = true; //true means low byte, false means high byte
    for(uint8_t i = 0; i < numberOfBytesRead; i++) {
      uint8_t wordAddress = i >> 1;
      uint8_t lowByteHighByteInstruction = isLowOrHigh ? 0x40 : 0x48;
      spiInstruction(lowByteHighByteInstruction, 0x00, wordAddress, pageBuffer[i]);
      
      isLowOrHigh = !isLowOrHigh;
    }

    Serial.println();

    uint8_t pageAddressHigher4Bits = (currentPage >> 3) &0x0F;
    uint8_t pageAddressLower3Bits = (currentPage & 0x07) << 5;
    spiInstruction(0x4C, pageAddressHigher4Bits, pageAddressLower3Bits, 0x00);
    currentPage++;

    delay(10);

    if (numberOfBytesRead < PAGE_BYTES) {
      Serial.println("Done programming ATMega");
      break; //it means we have fully read the bin file
    }
  }
  atmegaFile.close();
}

void loop() {
  // put your main code here, to run repeatedly:

}
