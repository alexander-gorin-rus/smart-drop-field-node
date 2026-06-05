#include <Arduino.h>
#include <Wire.h>
#include <SPI.h>
#include <Adafruit_SHT31.h>
#include <LoRa.h>
#include "esp_sleep.h"

#define BATTERY_ADC_PIN 0

#define BATTERY_DIVIDER_RATIO 2.0f
#define BATTERY_CALIBRATION 0.9125f
#define BATTERY_R1 10000.0f
#define BATTERY_R2 10000.0f
#define BATTERY_DIVIDER_RATIO ((BATTERY_R1 + BATTERY_R2) / BATTERY_R2)

#define NETWORK_ID "SD-TEST-0001"
#define NODE_ID 1

// ===== TEST MODE =====
#define TEST_MODE_DURATION_MS  (10UL * 60UL * 1000UL)
#define TEST_SEND_INTERVAL_MS  (60UL * 1000UL)

// ===== WORK MODE =====
#define WORK_SLEEP_MINUTES     30
#define ACK_TIMEOUT_MS         2500
#define MAX_SEND_ATTEMPTS      3
#define RETRY_DELAY_MS         700

// ===== I2C PINS =====
#define I2C_SDA_PIN 3
#define I2C_SCL_PIN 2

// ===== SOIL SENSOR =====
#define SOIL_SENSOR_PIN 4

#define SOIL_DRY_RAW 3030
#define SOIL_WET_RAW 1140

// ===== LORA SX1278 =====
#define LORA_SCK   6
#define LORA_MISO  10
#define LORA_MOSI  7

#define LORA_SS    20
#define LORA_RST   21
#define LORA_DIO0  1

Adafruit_SHT31 sht31 = Adafruit_SHT31();

uint16_t readBatteryMillivolts() {
  analogSetPinAttenuation(BATTERY_ADC_PIN, ADC_11db);
  delay(10);

  int raw = analogRead(BATTERY_ADC_PIN);

  float adcVoltage = (raw / 4095.0f) * 3300.0f;
  float batteryVoltage = adcVoltage * BATTERY_DIVIDER_RATIO * BATTERY_CALIBRATION;

  return (uint16_t)batteryVoltage;
}

uint8_t batteryPercentFromVoltage(uint16_t mv) {
  if (mv >= 4200) return 100;
  if (mv <= 3300) return 0;

  return (uint8_t)map(mv, 3300, 4200, 0, 100);
}

bool shtOk = false;
bool loraOk = false;

// Сохраняется между пробуждениями из deep sleep
RTC_DATA_ATTR uint32_t packetCounter = 1;
RTC_DATA_ATTR bool testModeCompleted = false;

unsigned long bootTime = 0;
unsigned long lastTestSendTime = 0;

int readSoilPercent(int rawValue) {
  int percent = map(rawValue, SOIL_DRY_RAW, SOIL_WET_RAW, 0, 100);
  return constrain(percent, 0, 100);
}

void printWakeupReason() {
  esp_sleep_wakeup_cause_t reason = esp_sleep_get_wakeup_cause();

  Serial.print("Wakeup reason: ");

  switch (reason) {
    case ESP_SLEEP_WAKEUP_TIMER:
      Serial.println("TIMER");
      break;

    case ESP_SLEEP_WAKEUP_UNDEFINED:
      Serial.println("POWER ON / RESET");
      break;

    default:
      Serial.println((int)reason);
      break;
  }
}

void initSensors() {
  Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN);

  shtOk = sht31.begin(0x44);

  if (shtOk) {
    Serial.println("SHT3X CONNECTED");
  } else {
    Serial.println("ERROR: SHT3X NOT FOUND");
  }
}

void initLoRa() {
  SPI.begin(
    LORA_SCK,
    LORA_MISO,
    LORA_MOSI,
    LORA_SS
  );

  LoRa.setPins(
    LORA_SS,
    LORA_RST,
    LORA_DIO0
  );

  loraOk = LoRa.begin(433E6);

  if (!loraOk) {
    Serial.println("ERROR: LORA INIT FAILED");
    return;
  }

  LoRa.setSyncWord(0x12);
  LoRa.setSpreadingFactor(7);
  LoRa.setSignalBandwidth(125E3);
  LoRa.setCodingRate4(5);
  LoRa.enableCrc();
  LoRa.setTxPower(17);

  Serial.println("LORA INIT SUCCESS");
}

void buildPacket(char *packet, size_t size) {
  uint16_t batteryMv = readBatteryMillivolts();

  Serial.print("Battery: ");
  Serial.print(batteryMv);
  Serial.println(" mV");
  uint8_t batteryPercent = batteryPercentFromVoltage(batteryMv);
  int soilRaw = analogRead(SOIL_SENSOR_PIN);
  int soilPercent = readSoilPercent(soilRaw);

  float temperature = -99.0f;
  float humidity = -1.0f;

  if (shtOk) {
    temperature = sht31.readTemperature();
    humidity = sht31.readHumidity();

    if (isnan(temperature) || isnan(humidity)) {
      Serial.println("ERROR: SHT3X READ FAILED");
      temperature = -99.0f;
      humidity = -1.0f;
    }
  }

  int temp10 = (int)(temperature * 10);
  int hum10 = (int)(humidity * 10);

  snprintf(
    packet,
    size,
    "NET:%s;NODE:%d;CNT:%lu;SOIL_RAW:%d;SOIL:%d;TEMP:%d.%d;HUM:%d.%d;BAT_MV:%u;BAT:%u",
    NETWORK_ID,
    NODE_ID,
    (unsigned long)packetCounter,
    soilRaw,
    soilPercent,
    temp10 / 10,
    abs(temp10 % 10),
    hum10 / 10,
    abs(hum10 % 10),
    batteryMv,
    batteryPercent
  );
}

uint32_t nextSleepSeconds = 30UL * 60UL;

String getFieldValue(const String &packet, const String &key) {
  String prefix = key + ":";

  int startIndex = packet.indexOf(prefix);
  if (startIndex == -1) return "";

  startIndex += prefix.length();

  int endIndex = packet.indexOf(';', startIndex);
  if (endIndex == -1) endIndex = packet.length();

  return packet.substring(startIndex, endIndex);
}

bool waitForAck(uint32_t expectedCounter) {
  unsigned long startTime = millis();

  while (millis() - startTime < ACK_TIMEOUT_MS) {
    int packetSize = LoRa.parsePacket();

    if (packetSize == 0) {
      delay(10);
      continue;
    }

    String response = "";

    while (LoRa.available()) {
      response += (char)LoRa.read();
    }

    Serial.print("LoRa response: ");
    Serial.println(response);

    String net = getFieldValue(response, "NET");
    String node = getFieldValue(response, "NODE");
    String cnt = getFieldValue(response, "CNT");
    String sleep = getFieldValue(response, "SLEEP");
    String mode = getFieldValue(response, "MODE");

    if (
      response.startsWith("ACK") &&
      net == NETWORK_ID &&
      node.toInt() == NODE_ID &&
      cnt.toInt() == expectedCounter
    ) {
      if (sleep.length() > 0) {
        nextSleepSeconds = sleep.toInt();
      }

      Serial.println("ACK RECEIVED");

      Serial.print("MODE: ");
      Serial.println(mode);

      Serial.print("NEXT SLEEP SECONDS: ");
      Serial.println(nextSleepSeconds);

      return true;
    }

    Serial.println("ACK IGNORED");
  }

  Serial.println("ACK TIMEOUT");
  return false;
}

bool sendPacketWithAck() {
  if (!loraOk) {
    Serial.println("LoRa not available");
    return false;
  }

  char packet[160];
  buildPacket(packet, sizeof(packet));

  Serial.println("========== SEND PACKET ==========");
  Serial.println(packet);

  for (int attempt = 1; attempt <= MAX_SEND_ATTEMPTS; attempt++) {
    Serial.print("Send attempt: ");
    Serial.println(attempt);

    LoRa.beginPacket();
    LoRa.print(packet);
    LoRa.endPacket();

    if (waitForAck(packetCounter)) {
      packetCounter++;
      return true;
    }

    delay(RETRY_DELAY_MS);
  }

  Serial.println("PACKET NOT CONFIRMED");
  packetCounter++;
  return false;
}

void goToSleep(uint32_t sleepSeconds) {
  if (sleepSeconds < 60) {
    sleepSeconds = 60;
  }

  Serial.print("Going to deep sleep for seconds: ");
  Serial.println(sleepSeconds);

  Serial.flush();

  if (loraOk) {
    LoRa.sleep();
  }

  esp_sleep_enable_timer_wakeup(
    (uint64_t)sleepSeconds * 1000000ULL
  );

  esp_deep_sleep_start();
}

void setup() {
  Serial.begin(115200);
  delay(2000);

  Serial.println();
  Serial.println("SMART DROP ESP32-C3 FIELD NODE");
  Serial.println("Firmware: test mode + ACK + sleep");

  printWakeupReason();

  bootTime = millis();

  initSensors();
  initLoRa();

  if (testModeCompleted) {
    Serial.println("WORK MODE");

    bool confirmed = sendPacketWithAck();

    if (!confirmed) {
      nextSleepSeconds = 30UL * 60UL;
    }

    goToSleep(nextSleepSeconds);
  } else {
    Serial.println("TEST MODE STARTED");
    Serial.println("Sending packet every 60 seconds for 10 minutes");
  }
}

void loop() {
  if (testModeCompleted) {
    return;
  }

  unsigned long now = millis();

  if (now - lastTestSendTime >= TEST_SEND_INTERVAL_MS) {
    lastTestSendTime = now;
    sendPacketWithAck();
  }

  if (now - bootTime >= TEST_MODE_DURATION_MS) {
    Serial.println("TEST MODE COMPLETED");
    testModeCompleted = true;

    bool confirmed = sendPacketWithAck();

    if (!confirmed) {
      nextSleepSeconds = 30UL * 60UL;
    }

    goToSleep(nextSleepSeconds);
  }
}