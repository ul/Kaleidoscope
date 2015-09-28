/*
  Copyright (c) 2014-2015 NicoHood
  See the readme for credit to other people.

  Advanced RawHID example

  Shows how to send bytes via RawHID.
  Press a button to send some example values.

  See HID Project documentation for more information.
  https://github.com/NicoHood/HID/wiki/RawHID-API
*/

#include "HID-Project.h"

const int pinLed = LED_BUILTIN;
const int pinButton = 2;

void setup() {
  pinMode(pinLed, OUTPUT);
  pinMode(pinButton, INPUT_PULLUP);
  Serial.begin(0);//TODO
  //Keyboard.begin();
  // No begin function needed for RawHID
}

void loop() {
  if (!digitalRead(pinButton)) {
    digitalWrite(pinLed, HIGH);

    // Direct without library. Always send RAWHID_RX_SIZE bytes!
    uint8_t buff[RAWHID_TX_SIZE] = {0};

    // With library
    memset(&buff, 42, sizeof(buff));
    RawHID.write(buff, sizeof(buff));

    // Write a single byte, will fill the rest with zeros
    RawHID.write(0xCD);

    // Huge buffer with library, will fill the rest with zeros
    uint8_t megabuff[100];
    for (int i = 0; i < sizeof(megabuff); i++)
      megabuff[i] = i;
    RawHID.write(megabuff, sizeof(megabuff));

    // You can use print too, but better do not use a linefeed.
    // A linefeed will send the \r and \n in a separate report.
    RawHID.println("Hello World");

    // Compare print to write:
    RawHID.write("Hello World\r\n");

    // Simple debounce
    delay(300);
    digitalWrite(pinLed, LOW);
  }

  uint8_t len = RawHID.available();
  if (len) {
    digitalWrite(pinLed, HIGH);

    // Mirror the incoming data from the host back
    uint8_t buff[len + 1];
    buff[0] = len;
    for (int i = 1; i < sizeof(buff); i++) {
      buff[i] = RawHID.read();
    }
    RawHID.write(buff, len);

    // Simple debounce
    delay(300);
    digitalWrite(pinLed, LOW);
  }
}

/*
  Expected output:

  // manual with unintialized buff
  recv 15 bytes:
  01 55 C1 FF 01 01 01 00 00 01 00 00 01 00 20

  // filled buff
  recv 15 bytes:
  2A 2A 2A 2A 2A 2A 2A 2A 2A 2A 2A 2A 2A 2A 2A

  // single byte filled with zero
  recv 15 bytes:
  CD 00 00 00 00 00 00 00 00 00 00 00 00 00 00

  // huge buffer filled with zero at the end
  recv 15 bytes:
  00 01 02 03 04 05 06 07 08 09 0A 0B 0C 0D 0E

  recv 15 bytes:
  0F 10 11 12 13 14 15 16 17 18 19 1A 1B 1C 1D

  recv 15 bytes:
  1E 1F 20 21 22 23 24 25 26 27 28 29 2A 2B 2C

  recv 15 bytes:
  2D 2E 2F 30 31 32 33 34 35 36 37 38 39 3A 3B

  recv 15 bytes:
  3C 3D 3E 3F 00 00 00 00 00 00 00 00 00 00 00

  // print
  recv 15 bytes:
  48 65 6C 6C 6F 20 57 6F 72 6C 64 00 00 00 00

  //\r
  recv 15 bytes:
  0D 00 00 00 00 00 00 00 00 00 00 00 00 00 00

  //\n
  recv 15 bytes:
  0A 00 00 00 00 00 00 00 00 00 00 00 00 00 00

  //write
  recv 15 bytes:
  48 65 6C 6C 6F 20 57 6F 72 6C 64 0D 0A 00 00

*/

