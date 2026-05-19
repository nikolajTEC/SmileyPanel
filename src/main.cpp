#include <Arduino.h>
#include "driver/rtc_io.h"

// Macro to convert a GPIO number into a 64-bit bitmask (1 shifted left by the GPIO number).
// Used by the ESP32 ext1 wakeup API, which expects a bitmask of wake-capable pins.
#define BUTTON_PIN_BITMASK(GPIO) (1ULL << GPIO)

// Conversion factor for microsecons to seconds
#define uS_TO_S_FACTOR 1000000ULL  
// Time ESP32 will go to sleep (in seconds)
#define TIME_TO_SLEEP  30        

// Struct pairing each physical button with:
//   wakePin   – the RTC GPIO that wakes the ESP32 from deep sleep
//   outputPin – the GPIO to pulse HIGH when this button triggers a wakeup
//   label     – human-readable name logged over Serial
struct ButtonPinMap {
  gpio_num_t  wakePin;
  uint8_t     outputPin;
  const char* label;
};

// Central pin table – add, remove, or rename buttons here only.
// Order doesn't matter; all entries are checked at wakeup.
static const ButtonPinMap BUTTONS[] = {
  { GPIO_NUM_35, 19, "Meget glad" },
  { GPIO_NUM_34, 21, "Glad"       },
  { GPIO_NUM_39, 22, "Sur"        },
  { GPIO_NUM_36, 23, "Meget sur"  },
};

// Number of entries in BUTTONS, computed at compile time.
// Avoids having to manually keep a count in sync with the table.
static const int BUTTON_COUNT = sizeof(BUTTONS) / sizeof(BUTTONS[0]);

// Builds the bitmask of all wake-capable pins by OR-ing each entry's bitmask together.
// Called once in setup() to pass to esp_sleep_enable_ext1_wakeup().
uint64_t buildWakeupBitmask() {
  uint64_t mask = 0;
  for (int i = 0; i < BUTTON_COUNT; i++) {
    mask |= BUTTON_PIN_BITMASK(BUTTONS[i].wakePin);
  }
  return mask;
}

// Persists across deep sleep cycles in RTC memory (normal RAM is wiped during deep sleep).
RTC_DATA_ATTR int bootCount = 0;

// Determines which button triggered the wakeup, pulses the corresponding output pin,
// and logs the result. If no known button is matched (e.g. first boot), logs a fallback message.
void handleWakeupButton() {
  // Bitmask of whichever GPIO(s) triggered the ext1 wakeup
  uint64_t wakeup_pin_mask = esp_sleep_get_ext1_wakeup_status();

  for (int i = 0; i < BUTTON_COUNT; i++) {
    if (wakeup_pin_mask & BUTTON_PIN_BITMASK(BUTTONS[i].wakePin)) {
      // Pulse the paired output pin HIGH for 1 second as a confirmation signal
      pinMode(BUTTONS[i].outputPin, OUTPUT);
      digitalWrite(BUTTONS[i].outputPin, HIGH);
      delay(1000);
      digitalWrite(BUTTONS[i].outputPin, LOW);

      Serial.printf("Bedømmelse: %s registreret\n", BUTTONS[i].label);
      return; // Only one button can wake the device; stop checking once found
    }
  }

  // Reached if wakeup_pin_mask is 0 (first boot) or an unexpected pin fired
  Serial.println("Wakeup button unknown (first boot or non-button wakeup)");
}

void setup() {
  Serial.begin(115200);
  delay(100); // Brief delay to ensure Serial is fully ready after waking up
  
  // Increment boot number for tracking
  bootCount++;

  // Read the wakeup reason to decide whether a button or the timer woke us up
  esp_sleep_wakeup_cause_t wakeup_reason = esp_sleep_get_wakeup_cause();

  if (wakeup_reason == ESP_SLEEP_WAKEUP_EXT1) {
    // Handles push of button
    handleWakeupButton();
  } 
  else if (wakeup_reason == ESP_SLEEP_WAKEUP_TIMER) {
    // Write to console im alive if woke up by the 30s timer
    Serial.println("im alive");
  } 
  else {
    // Fallback for initial power-on or reset
    Serial.printf("Initial boot count: %d\n", bootCount);
  }

  // Makes it so the buttons can wake it from deep sleep
  esp_sleep_enable_ext1_wakeup(buildWakeupBitmask(), ESP_EXT1_WAKEUP_ANY_HIGH);
  
  // Configure the timer to wake up the ESP32 every 30 seconds
  esp_sleep_enable_timer_wakeup(TIME_TO_SLEEP * uS_TO_S_FACTOR);

  // Starts deep sleep again, after all code has been executed
  esp_deep_sleep_start();
}

void loop() {}