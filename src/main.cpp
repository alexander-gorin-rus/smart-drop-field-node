#include <Arduino.h>
#include <SPI.h>
#include <LoRa.h>

// SX1278
#define LORA_SS     10
#define LORA_RST     9
#define LORA_DIO0    2

void setup() {

  Serial.begin(9600);

  while (!Serial) {}

  Serial.println();
  Serial.println("SMART DROP LORA TEST");

  LoRa.setPins(
    LORA_SS,
    LORA_RST,
    LORA_DIO0
  );

  if (!LoRa.begin(433E6)) {

    Serial.println("LORA INIT FAILED");

    while (true) {
      delay(1000);
    }
  }

  Serial.println("LORA INIT SUCCESS");
}

void loop() {

  Serial.println("Sending packet...");

  LoRa.beginPacket();

  LoRa.print("HELLO SMART DROP");

  LoRa.endPacket();

  Serial.println("Packet sent");

  delay(3000);
}