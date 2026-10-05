#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <DHT.h>
#include <qrcode.h>

#include "secrets.h"

constexpr uint8_t DHT_PIN = 4;
constexpr uint8_t PIR_PIN = 13;
constexpr uint8_t FC51_PIN = 27;
constexpr uint8_t LED_PIN = 21;

constexpr uint8_t FC51_DETECTED_LEVEL = LOW;

constexpr uint32_t PIR_CONFIRM_MS = 150;
constexpr uint32_t FC51_CONFIRM_MS = 50;
constexpr uint32_t LED_HOLD_MS = 5000;
constexpr uint32_t SENSOR_INTERVAL_MS = 2000;
constexpr uint32_t LED_BLINK_INTERVAL_MS = 250;
constexpr uint32_t EXIT_DELAY_MS = 5000;

DHT dht(DHT_PIN, DHT11);
WebServer server(80);

enum class SecurityMode : uint8_t { Disarmed, Home, Away, Test };

SecurityMode activeMode = SecurityMode::Disarmed;
SecurityMode targetMode = SecurityMode::Disarmed;
bool exitDelayActive = false;
bool alarmLatched = false;
bool sensorReadAttempted = false;
uint32_t exitDelayStartedMillis = 0;
uint32_t entryDelayStartedMillis = 0;

int pirCandidateState = LOW;
int confirmedPirState = LOW;

bool fc51Initialized = false;
bool fc51CandidateDetected = false;
bool fc51Detected = false;
bool alertActive = false;
bool alarmLedState = LOW;

uint32_t objectCount = 0;
float temperature = NAN;
float humidity = NAN;

uint32_t pirCandidateSince = 0;
uint32_t fc51CandidateSince = 0;
uint32_t lastObjectCountMillis = 0;
uint32_t previousSensorMillis = 0;
uint32_t previousLedMillis = 0;
bool hasCountedObject = false;

const char *modeName(SecurityMode mode) {
  switch (mode) {
    case SecurityMode::Home: return "home";
    case SecurityMode::Away: return "away";
    case SecurityMode::Test: return "test";
    default: return "disarmed";
  }
}

const char *modeStateName() {
  if (exitDelayActive) return "exit_delay";
  if (alarmLatched) return "alarm";
  if (activeMode == SecurityMode::Test) return "test";
  return activeMode == SecurityMode::Disarmed ? "disarmed" : "armed";
}

uint32_t modeSecondsRemaining(uint32_t now) {
  if (exitDelayActive) {
    const uint32_t elapsed = now - exitDelayStartedMillis;
    return elapsed >= EXIT_DELAY_MS ? 0 : (EXIT_DELAY_MS - elapsed + 999) / 1000;
  }
  return 0;
}

void addCorsHeaders() {
  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.sendHeader("Access-Control-Allow-Methods", "GET, POST, OPTIONS");
  server.sendHeader("Access-Control-Allow-Headers", "Content-Type");
}

void printQr(const char *text) {
  esp_qrcode_config_t config = {};
  config.display_func = esp_qrcode_print_console;
  config.max_qrcode_version = 10;
  config.qrcode_ecc_level = ESP_QRCODE_ECC_LOW;

  const esp_err_t result = esp_qrcode_generate(&config, text);
  if (result != ESP_OK) {
    Serial.printf("QR generation failed: %d\n", result);
  }
}

void handleStatus() {
  const uint32_t now = millis();
  const SecurityMode displayedMode = exitDelayActive ? targetMode : activeMode;
  String json = "{";
  json += "\"pir\":";
  json += confirmedPirState == HIGH ? "true" : "false";
  json += ",\"ir\":";
  json += fc51Detected ? "true" : "false";
  json += ",\"count\":";
  json += objectCount;
  json += ",\"temperature\":";
  json += isnan(temperature) ? "null" : String(temperature, 1);
  json += ",\"humidity\":";
  json += isnan(humidity) ? "null" : String(humidity, 1);
  json += ",\"led\":";
  json += alertActive ? "true" : "false";
  json += ",\"mode\":\"";
  json += modeName(displayedMode);
  json += "\",\"modeState\":\"";
  json += modeStateName();
  json += "\",\"modeRemainingSec\":";
  json += modeSecondsRemaining(now);
  json += ",\"alarm\":";
  json += alarmLatched ? "true" : "false";
  json += ",\"rssi\":";
  json += WiFi.status() == WL_CONNECTED ? String(WiFi.RSSI()) : String("null");
  json += ",\"sensorOk\":";
  json += !isnan(temperature) && !isnan(humidity) ? "true" : "false";
  json += ",\"sensorReady\":";
  json += sensorReadAttempted ? "true" : "false";
  json += ",\"sensorAgeMs\":";
  json += now - previousSensorMillis;
  json += ",\"uptime\":";
  json += millis() / 1000;
  json += "}";

  addCorsHeaders();
  server.sendHeader("Cache-Control", "no-store");
  server.send(200, "application/json", json);
}

void handleSetMode() {
  String requestedMode = server.arg("mode");
  requestedMode.trim();
  requestedMode.toLowerCase();

  if (requestedMode == "disarmed") {
    activeMode = SecurityMode::Disarmed;
    targetMode = SecurityMode::Disarmed;
    exitDelayActive = false;
    alarmLatched = false;
    alertActive = false;
    alarmLedState = LOW;
    digitalWrite(LED_PIN, LOW);
  } else if (requestedMode == "home" || requestedMode == "away") {
    targetMode = requestedMode == "home" ? SecurityMode::Home : SecurityMode::Away;
    activeMode = SecurityMode::Disarmed;
    exitDelayActive = true;
    exitDelayStartedMillis = millis();
    alarmLatched = false;
    alertActive = false;
    alarmLedState = LOW;
    digitalWrite(LED_PIN, LOW);
  } else if (requestedMode == "test") {
    activeMode = SecurityMode::Test;
    targetMode = SecurityMode::Test;
    exitDelayActive = false;
    alarmLatched = false;
    alertActive = false;
    alarmLedState = LOW;
    digitalWrite(LED_PIN, LOW);
  } else {
    addCorsHeaders();
    server.send(400, "text/plain", "Expected home, away, disarmed, or test");
    return;
  }

  addCorsHeaders();
  server.send(200, "application/json", "{\"ok\":true}");
}

void handleOptions() {
  addCorsHeaders();
  server.send(204);
}

void setup() {
  Serial.begin(115200);

  pinMode(PIR_PIN, INPUT);
  pinMode(FC51_PIN, INPUT_PULLUP);
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);

  dht.begin();

  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  server.on("/api/status", HTTP_GET, handleStatus);
  server.on("/api/mode", HTTP_POST, handleSetMode);
  server.on("/api/mode", HTTP_OPTIONS, handleOptions);
  server.begin();

  Serial.println();
  Serial.print("Dashboard QR code for: ");
  Serial.println(DASHBOARD_URL);
  printQr(DASHBOARD_URL);
}

void loop() {
  const uint32_t now = millis();

  server.handleClient();

  static bool printedDeviceIp = false;
  if (WiFi.status() == WL_CONNECTED && !printedDeviceIp) {
    Serial.print("ESP32 API: http://");
    Serial.println(WiFi.localIP());
    printedDeviceIp = true;
  }

  const int rawPirState = digitalRead(PIR_PIN);
  if (rawPirState != pirCandidateState) {
    pirCandidateState = rawPirState;
    pirCandidateSince = now;
  }
  if (pirCandidateState != confirmedPirState &&
      now - pirCandidateSince >= PIR_CONFIRM_MS) {
    confirmedPirState = pirCandidateState;
  }

  bool objectCountIncreased = false;
  const int rawFc51Level = digitalRead(FC51_PIN);
  const bool rawFc51Detected = rawFc51Level == FC51_DETECTED_LEVEL;

  if (!fc51Initialized) {
    fc51CandidateDetected = rawFc51Detected;
    fc51Detected = rawFc51Detected;
    fc51CandidateSince = now;
    fc51Initialized = true;
  } else {
    if (rawFc51Detected != fc51CandidateDetected) {
      fc51CandidateDetected = rawFc51Detected;
      fc51CandidateSince = now;
    }

    if (fc51CandidateDetected != fc51Detected &&
        now - fc51CandidateSince >= FC51_CONFIRM_MS) {
      fc51Detected = fc51CandidateDetected;
      if (fc51Detected) {
        ++objectCount;
        objectCountIncreased = true;
        lastObjectCountMillis = now;
        hasCountedObject = true;
        Serial.print("Object count: ");
        Serial.println(objectCount);
      }
    }
  }

  if (exitDelayActive && now - exitDelayStartedMillis >= EXIT_DELAY_MS) {
    activeMode = targetMode;
    exitDelayActive = false;
  }

  const bool systemArmed =
      (activeMode == SecurityMode::Home || activeMode == SecurityMode::Away) &&
      !exitDelayActive;
  const bool securitySensorTriggered =
      confirmedPirState == HIGH ||
      (activeMode == SecurityMode::Away && objectCountIncreased);

  if (systemArmed && securitySensorTriggered) {
    alarmLatched = true;
  }

  const bool testLedActive = activeMode == SecurityMode::Test &&
      (confirmedPirState == HIGH || fc51Detected);
  const bool newAlertActive = alarmLatched || testLedActive;

  if (newAlertActive) {
    if (!alertActive) {
      alarmLedState = HIGH;
      digitalWrite(LED_PIN, alarmLedState);
      previousLedMillis = now;
    } else if (now - previousLedMillis >= LED_BLINK_INTERVAL_MS) {
      previousLedMillis = now;
      alarmLedState = !alarmLedState;
      digitalWrite(LED_PIN, alarmLedState);
    }
  } else {
    alarmLedState = LOW;
    digitalWrite(LED_PIN, LOW);
  }
  alertActive = newAlertActive;

  if (now - previousSensorMillis >= SENSOR_INTERVAL_MS) {
    previousSensorMillis = now;
    humidity = dht.readHumidity();
    temperature = dht.readTemperature();
    sensorReadAttempted = true;

    Serial.println("----------------------------------------");
    Serial.print("WiFi: ");
    if (WiFi.status() == WL_CONNECTED) {
      Serial.print("connected, ESP32 IP: ");
      Serial.println(WiFi.localIP());
    } else {
      Serial.print("not connected, status code: ");
      Serial.println(static_cast<int>(WiFi.status()));
    }
    Serial.print("PIR motion: ");
    Serial.println(confirmedPirState == HIGH ? "detected" : "clear");
    Serial.print("FC-51 proximity: ");
    Serial.println(fc51Detected ? "detected" : "clear");
    Serial.print("FC-51 GPIO 27 raw level: ");
    Serial.println(rawFc51Level == HIGH ? "HIGH" : "LOW");
    Serial.print("Object count: ");
    Serial.println(objectCount);

    if (isnan(humidity) || isnan(temperature)) {
      Serial.println("Temperature/humidity: DHT11 read error.");
    } else {
      Serial.print("Temperature: ");
      Serial.print(temperature);
      Serial.print(" C | Humidity: ");
      Serial.print(humidity);
      Serial.println(" %");
    }
  }
}