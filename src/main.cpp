#include <Arduino.h>
#include <Wire.h>
#include <SPI.h>
#include <Adafruit_SHT31.h>

#define I2C_SDA_PIN 3
#define I2C_SCL_PIN 2
#define SOIL_SENSOR_PIN 4

#define SOIL_DRY_RAW 2630
#define SOIL_WET_RAW 1180

Adafruit_SHT31 sht31 = Adafruit_SHT31();

bool shtOk = false;

int readSoilPercent(int rawValue) {
  int percent = map(rawValue, SOIL_DRY_RAW, SOIL_WET_RAW, 0, 100);
  return constrain(percent, 0, 100);
}

void setup() {
  Serial.begin(115200);
  delay(3000);

  Serial.println();
  Serial.println("SMART DROP ESP32-C3 FIELD NODE");
  Serial.println("Sensors test only");

  Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN);

  shtOk = sht31.begin(0x44);

  if (shtOk) {
    Serial.println("SHT3X CONNECTED");
  } else {
    Serial.println("ERROR: SHT3X NOT FOUND");
    Serial.println("Will continue soil sensor test anyway");
  }

  Serial.println();
}

void loop() {
  int soilRaw = analogRead(SOIL_SENSOR_PIN);
  int soilPercent = readSoilPercent(soilRaw);

  Serial.println("========== SENSOR DATA ==========");

  Serial.print("Soil Raw: ");
  Serial.println(soilRaw);

  Serial.print("Soil Moisture: ");
  Serial.print(soilPercent);
  Serial.println(" %");

  if (shtOk) {
    float temperature = sht31.readTemperature();
    float humidity = sht31.readHumidity();

    Serial.print("Temperature: ");
    Serial.print(temperature, 1);
    Serial.println(" C");

    Serial.print("Humidity: ");
    Serial.print(humidity, 1);
    Serial.println(" %");
  } else {
    Serial.println("SHT3X: NOT CONNECTED");
  }

  Serial.println();
  delay(2000);
}