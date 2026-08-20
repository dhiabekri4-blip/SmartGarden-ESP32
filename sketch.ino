#define BLYNK_TEMPLATE_ID "TMPL4e-dRId2t"
#define BLYNK_TEMPLATE_NAME "SmartGarden"
#define BLYNK_AUTH_TOKEN "YOUR_BLYNK_AUTH_TOKEN"
#define BLYNK_PRINT Serial

#include <WiFi.h>
#include <BlynkSimpleEsp32.h>
#include "DHTesp.h"

// =========================
// WIFI
// =========================

char ssid[] = "Wokwi-GUEST";
char pass[] = "";

// =========================
// SYSTEM STATUS
// =========================

enum SystemStatus {
  NORMAL,
  WATERING,
  LOW_TANK,
  HIGH_TEMPERATURE,
  CRITICAL,
  SENSOR_FAULT
};

// =========================
// PLANT STATUS
// =========================

enum PlantStatus {
  HEALTHY,
  DRY,
  CRITICAL_DRY
};

// =========================
// FAULT CODES
// =========================

enum FaultCode {
  NO_FAULT,
  F01_DHT_SENSOR,
  F02_TOMATO_SENSOR,
  F03_MINT_SENSOR,
  F04_LOW_TANK
};

// =========================
// PLANT STRUCTURE
// =========================

struct PlantZone {
  const char* name;

  int soilPin;
  int pumpPin;

  int minMoisture;
  int healthyMoisture;

  bool needsWater;
  bool pumpRunning;

  unsigned long pumpStartTime;
  unsigned long lastWateringTime;

  int moisture;

  PlantStatus status;

  int sensorErrorCount;
  bool sensorFault;
};

// =========================
// PLANT ZONES
// =========================

PlantZone tomato = {
  "TOMATOES",
  34,          // Soil sensor
  26,          // Pump
  45,          // Start watering below 45%
  50,          // Healthy again at 50%
  false,
  false,
  0,
  0,
  0,
  HEALTHY,
  0,
  false
};

PlantZone mint = {
  "MINT",
  32,          // Soil sensor
  27,          // Pump
  30,          // Start watering below 30%
  35,          // Healthy again at 35%
  false,
  false,
  0,
  0,
  0,
  HEALTHY,
  0,
  false
};

// =========================
// SENSOR PINS
// =========================

const int dhtPin = 15;
const int tankPin = 35;

DHTesp dhtSensor;

// =========================
// TIMING
// =========================

const unsigned long wateringDuration = 3000;
const unsigned long cooldownDuration = 10000;

const unsigned long dhtInterval = 2000;
const unsigned long displayInterval = 2000;

unsigned long lastDhtRead = 0;
unsigned long lastDisplayTime = 0;

// =========================
// SENSOR VALUES
// =========================

float temperature = 0;
float humidity = 0;

int tankPercent = 0;

bool lowTank = false;
bool autoMode = true;

// =========================
// SENSOR DIAGNOSTICS
// =========================

bool dhtFault = false;

int dhtErrorCount = 0;

const int maxDhtErrors = 3;
const int maxSoilErrors = 5;

// =========================
// SETUP
// =========================

void setup() {
  Serial.begin(115200);

  Blynk.begin(BLYNK_AUTH_TOKEN, ssid, pass);

  // Restore AUTO/MANUAL state from Blynk
  Blynk.syncVirtual(V8);

  pinMode(tomato.pumpPin, OUTPUT);
  pinMode(mint.pumpPin, OUTPUT);

  digitalWrite(tomato.pumpPin, LOW);
  digitalWrite(mint.pumpPin, LOW);

  dhtSensor.setup(dhtPin, DHTesp::DHT22);
}

// =========================
// BLYNK - AUTO / MANUAL
// V8: 1 = AUTO, 0 = MANUAL
// =========================

BLYNK_WRITE(V8) {
  autoMode = param.asInt();

  Serial.print("CONTROL MODE: ");

  if (autoMode) {
    Serial.println("AUTO");
  } else {
    Serial.println("MANUAL");
  }
}

// =========================
// BLYNK - MANUAL TOMATO PUMP
// V9
// =========================

BLYNK_WRITE(V9) {
  int command = param.asInt();

  // Manual control is disabled in AUTO mode
  if (autoMode) {
    Blynk.virtualWrite(V9, 0);
    return;
  }

  if (command == 1 &&
      !lowTank &&
      !tomato.sensorFault) {

    digitalWrite(tomato.pumpPin, HIGH);

    tomato.pumpRunning = true;
    tomato.pumpStartTime = millis();
  }
  else {
    digitalWrite(tomato.pumpPin, LOW);

    tomato.pumpRunning = false;
    tomato.lastWateringTime = millis();
  }
}

// =========================
// BLYNK - MANUAL MINT PUMP
// V10
// =========================

BLYNK_WRITE(V10) {
  int command = param.asInt();

  // Manual control is disabled in AUTO mode
  if (autoMode) {
    Blynk.virtualWrite(V10, 0);
    return;
  }

  if (command == 1 &&
      !lowTank &&
      !mint.sensorFault) {

    digitalWrite(mint.pumpPin, HIGH);

    mint.pumpRunning = true;
    mint.pumpStartTime = millis();
  }
  else {
    digitalWrite(mint.pumpPin, LOW);

    mint.pumpRunning = false;
    mint.lastWateringTime = millis();
  }
}

// =========================
// CONTROL ONE PLANT ZONE
// =========================

void controlZone(PlantZone &zone, bool lowTank) {

  // Read soil sensor
  int rawValue = analogRead(zone.soilPin);

  // Sensor diagnostics
  if (rawValue <= 5 || rawValue >= 4090) {
    zone.sensorErrorCount++;

    if (zone.sensorErrorCount >= maxSoilErrors) {
      zone.sensorFault = true;
    }
  }
  else {
    zone.sensorErrorCount = 0;
    zone.sensorFault = false;
  }

  // Convert ADC value to percentage
  zone.moisture = map(
    rawValue,
    0,
    4095,
    0,
    100
  );

  // Hysteresis
  if (zone.moisture < zone.minMoisture) {
    zone.needsWater = true;
  }
  else if (zone.moisture >= zone.healthyMoisture) {
    zone.needsWater = false;
  }

  // Plant status
  if (zone.moisture < 20) {
    zone.status = CRITICAL_DRY;
  }
  else if (zone.needsWater) {
    zone.status = DRY;
  }
  else {
    zone.status = HEALTHY;
  }

  // Stop pump if soil sensor fails
  if (zone.sensorFault && zone.pumpRunning) {
    digitalWrite(zone.pumpPin, LOW);

    zone.pumpRunning = false;
    zone.lastWateringTime = millis();

    return;
  }

  // Emergency stop if tank is low
  if (lowTank && zone.pumpRunning) {
    digitalWrite(zone.pumpPin, LOW);

    zone.pumpRunning = false;
    zone.lastWateringTime = millis();

    return;
  }

  // Stop pump after 3 seconds
 if (
  zone.pumpRunning &&
  millis() - zone.pumpStartTime >= wateringDuration
) {
  digitalWrite(zone.pumpPin, LOW);

  zone.pumpRunning = false;
  zone.lastWateringTime = millis();

  if (!autoMode) {
    if (zone.pumpPin == tomato.pumpPin) {
      Blynk.virtualWrite(V9, 0);
    }

    if (zone.pumpPin == mint.pumpPin) {
      Blynk.virtualWrite(V10, 0);
    }
  }
}

  // Check watering cooldown
  bool cooldownFinished =
    zone.lastWateringTime == 0 ||
    millis() - zone.lastWateringTime >= cooldownDuration;

  // Automatic irrigation
  if (
    autoMode &&
    zone.needsWater &&
    !zone.sensorFault &&
    !lowTank &&
    !zone.pumpRunning &&
    cooldownFinished
  ) {
    digitalWrite(zone.pumpPin, HIGH);

    zone.pumpRunning = true;
    zone.pumpStartTime = millis();
  }
}

// =========================
// GET FAULT CODE
// =========================

FaultCode getFaultCode() {

  if (dhtFault) {
    return F01_DHT_SENSOR;
  }

  if (tomato.sensorFault) {
    return F02_TOMATO_SENSOR;
  }

  if (mint.sensorFault) {
    return F03_MINT_SENSOR;
  }

  if (lowTank) {
    return F04_LOW_TANK;
  }

  return NO_FAULT;
}

// =========================
// DISPLAY FAULT CODE
// =========================

void displayFaultCode(FaultCode fault) {

  switch (fault) {

    case NO_FAULT:
      Serial.println("FAULT CODE: NONE");
      break;

    case F01_DHT_SENSOR:
      Serial.println("FAULT CODE: F01");
      Serial.println("FAULT: DHT22 SENSOR FAILURE");
      break;

    case F02_TOMATO_SENSOR:
      Serial.println("FAULT CODE: F02");
      Serial.println("FAULT: TOMATO SOIL SENSOR FAILURE");
      break;

    case F03_MINT_SENSOR:
      Serial.println("FAULT CODE: F03");
      Serial.println("FAULT: MINT SOIL SENSOR FAILURE");
      break;

    case F04_LOW_TANK:
      Serial.println("FAULT CODE: F04");
      Serial.println("FAULT: LOW WATER TANK");
      break;
  }
}

// =========================
// DISPLAY PLANT ZONE
// =========================

void displayZone(PlantZone &zone) {

  Serial.print("----- ");
  Serial.print(zone.name);
  Serial.println(" -----");

  Serial.print("Moisture: ");
  Serial.print(zone.moisture);
  Serial.println("%");

  Serial.print("SOIL STATUS: ");

  switch (zone.status) {

    case HEALTHY:
      Serial.println("HEALTHY");
      break;

    case DRY:
      Serial.println("DRY");
      break;

    case CRITICAL_DRY:
      Serial.println("CRITICAL DRY");
      break;
  }

  Serial.print("SENSOR STATUS: ");

  if (zone.sensorFault) {
    Serial.println("FAULT");
  }
  else {
    Serial.println("OK");
  }

  Serial.print("PUMP: ");

  if (zone.pumpRunning) {
    Serial.println("ON");
  }
  else {
    Serial.println("OFF");
  }

  if (zone.sensorFault) {
    Serial.println("IRRIGATION: BLOCKED - SENSOR FAULT");
  }

  if (zone.needsWater && lowTank) {
    Serial.println("IRRIGATION: BLOCKED - LOW TANK");
  }

  if (
    zone.needsWater &&
    !zone.sensorFault &&
    !zone.pumpRunning &&
    !lowTank &&
    zone.lastWateringTime != 0 &&
    millis() - zone.lastWateringTime < cooldownDuration
  ) {
    Serial.println("IRRIGATION: COOLDOWN");
  }

  Serial.println();
}

// =========================
// SYSTEM STATUS
// =========================

SystemStatus checkSystemStatus() {

  bool sensorFailure =
    dhtFault ||
    tomato.sensorFault ||
    mint.sensorFault;

  bool criticalSoil =
    tomato.status == CRITICAL_DRY ||
    mint.status == CRITICAL_DRY;

  bool anyPumpRunning =
    tomato.pumpRunning ||
    mint.pumpRunning;

  if (sensorFailure) {
    return SENSOR_FAULT;
  }

  if (criticalSoil) {
    return CRITICAL;
  }

  if (lowTank) {
    return LOW_TANK;
  }

  if (!dhtFault && temperature > 35) {
    return HIGH_TEMPERATURE;
  }

  if (anyPumpRunning) {
    return WATERING;
  }

  return NORMAL;
}

// =========================
// DISPLAY SYSTEM STATUS
// =========================

void displaySystemStatus(SystemStatus status) {

  Serial.print("SYSTEM STATUS: ");

  switch (status) {

    case NORMAL:
      Serial.println("NORMAL");
      break;

    case WATERING:
      Serial.println("WATERING");
      break;

    case LOW_TANK:
      Serial.println("LOW TANK");
      break;

    case HIGH_TEMPERATURE:
      Serial.println("HIGH TEMPERATURE");
      break;

    case CRITICAL:
      Serial.println("CRITICAL");
      break;

    case SENSOR_FAULT:
      Serial.println("SENSOR FAULT");
      break;
  }
}

// =========================
// MAIN LOOP
// =========================

void loop() {

  Blynk.run();

  unsigned long currentTime = millis();

  // Read DHT22 every 2 seconds
  if (currentTime - lastDhtRead >= dhtInterval) {

    TempAndHumidity data =
      dhtSensor.getTempAndHumidity();

    if (
      isnan(data.temperature) ||
      isnan(data.humidity)
    ) {
      dhtErrorCount++;

      if (dhtErrorCount >= maxDhtErrors) {
        dhtFault = true;
      }
    }
    else {
      dhtErrorCount = 0;
      dhtFault = false;

      temperature = data.temperature;
      humidity = data.humidity;
    }

    lastDhtRead = currentTime;
  }

  // Read water tank
  int tankRaw = analogRead(tankPin);

  tankPercent = map(
    tankRaw,
    0,
    4095,
    0,
    100
  );

  lowTank = tankPercent < 10;

  // Control both plant zones
  controlZone(tomato, lowTank);
  controlZone(mint, lowTank);

  // Display + send data every 2 seconds
  if (currentTime - lastDisplayTime >= displayInterval) {

    lastDisplayTime = currentTime;

    SystemStatus systemStatus =
      checkSystemStatus();

    FaultCode fault =
      getFaultCode();

    Serial.println();
    Serial.println("========== SMARTGARDEN ==========");

    Serial.print("CONTROL MODE: ");
    Serial.println(autoMode ? "AUTO" : "MANUAL");

    Serial.print("Temperature: ");

    if (dhtFault) {
      Serial.println("SENSOR ERROR");
    }
    else {
      Serial.print(temperature);
      Serial.println(" C");
    }

    Serial.print("Humidity: ");

    if (dhtFault) {
      Serial.println("SENSOR ERROR");
    }
    else {
      Serial.print(humidity);
      Serial.println("%");
    }

    Serial.print("Tank Level: ");
    Serial.print(tankPercent);
    Serial.println("%");

    displaySystemStatus(systemStatus);
    displayFaultCode(fault);

    if (!dhtFault && temperature > 35) {
      Serial.println("WARNING: HIGH TEMPERATURE");
    }

    Serial.println();

    displayZone(tomato);
    displayZone(mint);

    Serial.println("=================================");
    Serial.println();

    // Send sensor data to Blynk
    Blynk.virtualWrite(V0, tomato.moisture);
    Blynk.virtualWrite(V1, mint.moisture);
    Blynk.virtualWrite(V2, dhtFault ? 0 : temperature);
    Blynk.virtualWrite(V3, dhtFault ? 0 : humidity);
    Blynk.virtualWrite(V4, tankPercent);

    // Pump status
    Blynk.virtualWrite(V5, tomato.pumpRunning ? 1 : 0);
    Blynk.virtualWrite(V6, mint.pumpRunning ? 1 : 0);

    // System status
    Blynk.virtualWrite(V7, (int)systemStatus);
  }

  delay(10);
}