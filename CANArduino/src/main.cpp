// Copyright (c) Sandeep Mistry. All rights reserved.
// Licensed under the MIT license. See LICENSE file in the project root for full license information.

#include <CAN.h>

void onReceive(int packetSize);

void setup() {
  Serial.begin(115200);
  while (!Serial);

  Serial.println("CAN Receiver Callback");

  CAN.setClockFrequency(8E6);

  // start the CAN bus at 500 kbps
  if (!CAN.begin(1000E3)) {
    Serial.println("Starting CAN failed!");
    while (1);
  }

  CAN.onReceive(onReceive);

}

void loop() {
  // send packet: id is 11 bits, packet can contain up to 8 bytes of data
  // Serial.print("Sending packet ... ");
  // Serial.println(millis());

  // CAN.beginPacket(0x12);
  // CAN.println(millis());
  // CAN.endPacket();

  // Serial.println("done");

  // delay(1000);
}

void onReceive(int packetSize) {
  // received a packet
  Serial.print("Received ");

  if (CAN.packetExtended()) {
    Serial.print("extended ");
  }

  if (CAN.packetRtr()) {
    // Remote transmission request, packet contains no data
    Serial.print("RTR ");
  }

  Serial.print("packet with id 0x");
  Serial.print(CAN.packetId(), HEX);

  if (CAN.packetRtr()) {
    Serial.print(" and requested length ");
    Serial.println(CAN.packetDlc());
  } else {
    Serial.print(" and length ");
    Serial.println(packetSize);

    // only print packet data for non-RTR packets
    while (CAN.available()) {
      Serial.print(CAN.read(), HEX);
    }
    Serial.println();
  }

  Serial.println();
}

