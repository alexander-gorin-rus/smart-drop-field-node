#include <Arduino.h>
#include <SPI.h>
#include <Wire.h>
#include <LoRa.h>
#include <Adafruit_SHT31.h>

// ===== NODE CONFIG =====
#define NODE_ID 1

// ===== SOIL SENSOR =====
#define SOIL_SENSOR_PIN A0

// Calibration values
#define SOIL_DRY_RAW 430
#define SOIL_WET_RAW 170

// ===== LORA SX1278 =====
#define LORA_SS   10
#define LORA_RST  9
#define LORA_DIO0 2

Adafruit_SHT31 sht31 = Adafruit_SHT31();

int readSoilPercent(int rawValue) {
  int percent = map(rawValue, SOIL_DRY_RAW, SOIL_WET_RAW, 0, 100);
  return constrain(percent, 0, 100);
}

void setup() {
  Serial.begin(9600);
  delay(1000);

  Serial.println();
  Serial.println("SMART DROP FIELD NODE");
  Serial.println("Sensors + LoRa test");

  Wire.begin();

  if (!sht31.begin(0x44)) {
    Serial.println("ERROR: SHT3X NOT FOUND");
    while (true) {
      delay(1000);
    }
  }

  Serial.println("SHT3X CONNECTED");

  LoRa.setPins(LORA_SS, LORA_RST, LORA_DIO0);

  if (!LoRa.begin(433E6)) {
    Serial.println("ERROR: LORA INIT FAILED");
    while (true) {
      delay(1000);
    }
  }

  Serial.println("LORA INIT SUCCESS");
  Serial.println();
}

void loop() {
  int soilRaw = analogRead(SOIL_SENSOR_PIN);
  int soilPercent = readSoilPercent(soilRaw);

  float temperature = sht31.readTemperature();
  float humidity = sht31.readHumidity();

  if (isnan(temperature) || isnan(humidity)) {
    Serial.println("ERROR: SHT3X READ FAILED");
    delay(5000);
    return;
  }

  String packet = "";
  packet += "NODE:";
  packet += NODE_ID;
  packet += ";SOIL_RAW:";
  packet += soilRaw;
  packet += ";SOIL:";
  packet += soilPercent;
  packet += ";TEMP:";
  packet += String(temperature, 1);
  packet += ";HUM:";
  packet += String(humidity, 1);

  Serial.println("========== SENSOR DATA ==========");
  Serial.print("Soil Raw: ");
  Serial.println(soilRaw);

  Serial.print("Soil Moisture: ");
  Serial.print(soilPercent);
  Serial.println(" %");

  Serial.print("Temperature: ");
  Serial.print(temperature, 1);
  Serial.println(" C");

  Serial.print("Humidity: ");
  Serial.print(humidity, 1);
  Serial.println(" %");

  Serial.print("LoRa Packet: ");
  Serial.println(packet);

  LoRa.beginPacket();
  LoRa.print(packet);
  LoRa.endPacket();

  Serial.println("LoRa packet sent");
  Serial.println();

  delay(5000);
}