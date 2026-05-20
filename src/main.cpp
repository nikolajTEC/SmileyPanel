#include <Arduino.h>
#include "driver/rtc_io.h"
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>
#include <time.h>
#include "secrets.h"   // WiFi + MQTT credentials
#include "ca_cert.h"   // MQTT_CA_CERT (broker's CA certificate)

// ── MQTT topic ────────────────────────────────────────────────────────────────
// Built at compile time: "/devices/device01/Gruppe2ESP"
// Change the suffix here if the group name ever changes.
#define MQTT_TOPIC       "/devices/" DEVICE_NAME "/Gruppe2ESP"
#define MQTT_CLIENT_ID   DEVICE_NAME "_esp32"

// ── NTP / timezone ────────────────────────────────────────────────────────────
#define TIMEZONE         "CET-1CEST,M3.5.0,M10.5.0/3"   // Europe/Copenhagen
#define NTP_SERVER       "pool.ntp.org"
#define NTP_SYNC_TIMEOUT_MS 8000

// ── Sleep config ──────────────────────────────────────────────────────────────
#define uS_TO_S_FACTOR   1000000ULL
#define TIME_TO_SLEEP    30

// ── Button pin table ──────────────────────────────────────────────────────────
#define BUTTON_PIN_BITMASK(GPIO) (1ULL << GPIO)

struct ButtonPinMap {
  gpio_num_t  wakePin;
  uint8_t     outputPin;
  const char* label;
};

static const ButtonPinMap BUTTONS[] = {
  { GPIO_NUM_35, 19, "Meget glad" },
  { GPIO_NUM_34, 21, "Glad"       },
  { GPIO_NUM_39, 22, "Sur"        },
  { GPIO_NUM_36, 23, "Meget sur"  },
};
static const int BUTTON_COUNT = sizeof(BUTTONS) / sizeof(BUTTONS[0]);

// ── RTC-persisted state ───────────────────────────────────────────────────────
RTC_DATA_ATTR int bootCount = 0;

// ── Network clients ───────────────────────────────────────────────────────────
WiFiClientSecure secureClient;
PubSubClient     mqttClient(secureClient);

// =============================================================================
//  Helpers
// =============================================================================

uint64_t buildWakeupBitmask() {
  uint64_t mask = 0;
  for (int i = 0; i < BUTTON_COUNT; i++) {
    mask |= BUTTON_PIN_BITMASK(BUTTONS[i].wakePin);
  }
  return mask;
}

bool connectWiFi() {
  Serial.printf("Connecting to WiFi: %s", WIFI_SSID);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED) {
    if (millis() - start > 10000) {
      Serial.println("\nWiFi timeout!");
      return false;
    }
    delay(250);
    Serial.print(".");
  }
  Serial.printf("\nWiFi connected. IP: %s\n", WiFi.localIP().toString().c_str());
  return true;
}

bool syncNTP() {
  configTzTime(TIMEZONE, NTP_SERVER);
  Serial.print("Syncing NTP");

  struct tm ti{};
  unsigned long start = millis();
  while (!getLocalTime(&ti) || ti.tm_year < (2020 - 1900)) {
    if (millis() - start > NTP_SYNC_TIMEOUT_MS) {
      Serial.println("\nNTP sync timeout!");
      return false;
    }
    delay(200);
    Serial.print(".");
  }

  char buf[32];
  strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", &ti);
  Serial.printf("\nNTP synced: %s (Copenhagen)\n", buf);
  return true;
}

// Returns e.g. "2025-05-20 14:32:07"
String getCPHTimestamp() {
  struct tm ti{};
  if (!getLocalTime(&ti)) return "1970-01-01 00:00:00";
  char buf[24];
  strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", &ti);
  return String(buf);
}

bool connectMQTT() {
  // Use the CA cert from ca_cert.h to verify the broker's TLS certificate
  secureClient.setCACert(MQTT_CA_CERT);

  mqttClient.setServer(MQTT_HOST, MQTT_PORT);
  mqttClient.setBufferSize(512);

  Serial.printf("Connecting to MQTT %s:%d as %s ...\n", MQTT_HOST, MQTT_PORT, MQTT_USER);

  // MQTT_PASS comes from secrets.h
  if (mqttClient.connect(MQTT_CLIENT_ID, MQTT_USER, MQTT_PASS)) {
    Serial.println("MQTT connected.");
    return true;
  }

  Serial.printf("MQTT connect failed. State: %d\n", mqttClient.state());
  return false;
}

void handleWakeupButton() {
  uint64_t wakeup_pin_mask = esp_sleep_get_ext1_wakeup_status();

  for (int i = 0; i < BUTTON_COUNT; i++) {
    if (!(wakeup_pin_mask & BUTTON_PIN_BITMASK(BUTTONS[i].wakePin))) continue;

    // ── 1. Pulse confirmation output ─────────────────────────────────────
    pinMode(BUTTONS[i].outputPin, OUTPUT);
    digitalWrite(BUTTONS[i].outputPin, HIGH);
    delay(1000);
    digitalWrite(BUTTONS[i].outputPin, LOW);

    Serial.printf("Bedømmelse: %s registreret\n", BUTTONS[i].label);

    // ── 2. Connect and publish ────────────────────────────────────────────
    if (!connectWiFi())  break;
    if (!syncNTP())      break;
    if (!connectMQTT())  break;

    // Payload: {"label":"Glad","timestamp":"2025-05-20 14:32:07"}
    char payload[128];
    snprintf(payload, sizeof(payload),
             "{\"label\":\"%s\",\"timestamp\":\"%s\"}",
             BUTTONS[i].label, getCPHTimestamp().c_str());

    if (mqttClient.publish(MQTT_TOPIC, payload, /*retained=*/false)) {
      Serial.printf("Published → %s : %s\n", MQTT_TOPIC, payload);
    } else {
      Serial.println("MQTT publish failed.");
    }

    mqttClient.disconnect();
    WiFi.disconnect(true);
    delay(100); // Let the stack flush before deep sleep
    break;      // Only one button wakes the device at a time
  }

  if (!wakeup_pin_mask) {
    Serial.println("Wakeup button unknown (first boot or non-button wakeup)");
  }
}

// =============================================================================
//  Arduino entry points
// =============================================================================

void setup() {
  Serial.begin(115200);
  delay(100);

  bootCount++;

  esp_sleep_wakeup_cause_t wakeup_reason = esp_sleep_get_wakeup_cause();

  if (wakeup_reason == ESP_SLEEP_WAKEUP_EXT1) {
    handleWakeupButton();        // Button press → network + publish
  } else if (wakeup_reason == ESP_SLEEP_WAKEUP_TIMER) {
    Serial.println("im alive"); // Heartbeat, no network needed
  } else {
    Serial.printf("Initial boot. Count: %d\n", bootCount);
  }

  esp_sleep_enable_ext1_wakeup(buildWakeupBitmask(), ESP_EXT1_WAKEUP_ANY_HIGH);
  esp_sleep_enable_timer_wakeup(TIME_TO_SLEEP * uS_TO_S_FACTOR);
  esp_deep_sleep_start();
}

void loop() {}