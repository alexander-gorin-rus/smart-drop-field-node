#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_SHT31.h>

#define SOIL_SENSOR_PIN A0

Adafruit_SHT31 sht31 = Adafruit_SHT31();

void setup() {
  Serial.begin(9600);
  delay(1000);

  Serial.println();
  Serial.println("SMART DROP SENSOR TEST");

  Wire.begin();

  if (!sht31.begin(0x44)) {
    Serial.println("ERROR: SHT3X NOT FOUND");

    while (true) {
      delay(1000);
    }
  }

  Serial.println("SHT3X CONNECTED");
  Serial.println();
}

void loop() {

  // ===== SOIL SENSOR =====
  int soilRaw = analogRead(SOIL_SENSOR_PIN);

  // Калибровка
  int soilPercent = map(soilRaw, 430, 170, 0, 100);

  // Ограничение диапазона
  soilPercent = constrain(soilPercent, 0, 100);

  // ===== AIR SENSOR =====
  float temperature = sht31.readTemperature();
  float humidity = sht31.readHumidity();

  // ===== OUTPUT =====
  Serial.println("========== SENSOR DATA ==========");

  Serial.print("Soil Raw: ");
  Serial.println(soilRaw);

  Serial.print("Soil Moisture: ");
  Serial.print(soilPercent);
  Serial.println(" %");

  if (isnan(temperature) || isnan(humidity)) {

    Serial.println("SHT3X READ ERROR");

  } else {

    Serial.print("Temperature: ");
    Serial.print(temperature);
    Serial.println(" C");

    Serial.print("Humidity: ");
    Serial.print(humidity);
    Serial.println(" %");
  }

  Serial.println();
  delay(2000);
}