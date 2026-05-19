#include <Arduino.h>

/*  
  Rui Santos & Sara Santos - Random Nerd Tutorials
  https://RandomNerdTutorials.com/learn-esp32-with-arduino-ide/
  Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files.
  The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.
*/

#include "driver/rtc_io.h"

#define BUTTON_PIN_BITMASK(GPIO) (1ULL << GPIO)  // 2 ^ GPIO_NUMBER in hex
// #define WAKEUP_GPIO_2              GPIO_NUM_2     // Only RTC IO are allowed - ESP32 Pin example
#define WAKEUP_GPIO_15              GPIO_NUM_15     // Only RTC IO are allowed - ESP32 Pin example
// #define WAKEUP_GPIO_4               GPIO_NUM_4
#define WAKEUP_GPIO_36              GPIO_NUM_36
#define WAKEUP_GPIO_39              GPIO_NUM_39
#define WAKEUP_GPIO_34              GPIO_NUM_34
#define WAKEUP_GPIO_35              GPIO_NUM_35 

#define OUTPUT_GPIO_23 23  // <-- added
#define OUTPUT_GPIO_22 22
#define OUTPUT_GPIO_21 21
#define OUTPUT_GPIO_19 19

#ifndef BUILTIN_LED
#define BUILTIN_LED 2
#endif

// #define buttonPin 15
#define EXTRA_LED 4
// Define bitmask for multiple GPIOs
// uint64_t bitmask = BUTTON_PIN_BITMASK(WAKEUP_GPIO_4) | BUTTON_PIN_BITMASK(WAKEUP_GPIO_15);
uint64_t bitmask = BUTTON_PIN_BITMASK(WAKEUP_GPIO_36) | BUTTON_PIN_BITMASK(WAKEUP_GPIO_39) | BUTTON_PIN_BITMASK(WAKEUP_GPIO_34) | BUTTON_PIN_BITMASK(WAKEUP_GPIO_35);
RTC_DATA_ATTR int bootCount = 0;

/*
Method to print the GPIO that triggered the wakeup
*/
void print_GPIO_wake_up(){
  int GPIO_reason = esp_sleep_get_ext1_wakeup_status();
  Serial.print("GPIO that triggered the wake up: GPIO ");
  Serial.println((log(GPIO_reason))/log(2), 0);
}


void print_wakeup_button() {
  uint64_t wakeup_pin_mask = esp_sleep_get_ext1_wakeup_status();
  if (wakeup_pin_mask & BUTTON_PIN_BITMASK(WAKEUP_GPIO_35)) {
     pinMode(OUTPUT_GPIO_19, OUTPUT);       // <-- added
    digitalWrite(OUTPUT_GPIO_19, HIGH);    // <-- added
    delay(1000);
    digitalWrite(OUTPUT_GPIO_19, LOW);
    Serial.println("Woken by Button 1 (GPIO 35)");
  } else if (wakeup_pin_mask & BUTTON_PIN_BITMASK(WAKEUP_GPIO_34)) {
         pinMode(OUTPUT_GPIO_21, OUTPUT);       // <-- added
    digitalWrite(OUTPUT_GPIO_21, HIGH);    // <-- added
    delay(1000);
    digitalWrite(OUTPUT_GPIO_21, LOW);
    Serial.println("Woken by Button 2 (GPIO 34)");
  } else if (wakeup_pin_mask & BUTTON_PIN_BITMASK(WAKEUP_GPIO_39)) {
     pinMode(OUTPUT_GPIO_22, OUTPUT);       // <-- added
    digitalWrite(OUTPUT_GPIO_22, HIGH);    // <-- added
    delay(1000);
    digitalWrite(OUTPUT_GPIO_22, LOW);
    Serial.println("Woken by Button 3 (GPIO 39)");
    } else if (wakeup_pin_mask & BUTTON_PIN_BITMASK(WAKEUP_GPIO_36)) {
           pinMode(OUTPUT_GPIO_23, OUTPUT);       // <-- added
    digitalWrite(OUTPUT_GPIO_23, HIGH);    // <-- added
    delay(1000);
    digitalWrite(OUTPUT_GPIO_23, LOW);
    Serial.println("Woken by Button 4 (GPIO 36)");
  } else {
    Serial.println("Wakeup button unknown (first boot or non-button wakeup)");
  }
}

/*
  Method to print the reason by which ESP32 has been awaken from sleep
*/


void print_wakeup_reason() {
  esp_sleep_wakeup_cause_t wakeup_reason;

  wakeup_reason = esp_sleep_get_wakeup_cause();

  switch (wakeup_reason) {
    case ESP_SLEEP_WAKEUP_EXT0:     
      Serial.println("Wakeup caused by external signal using RTC_IO");
      break;
    case ESP_SLEEP_WAKEUP_EXT1:
      Serial.println("Wakeup caused by external signal using RTC_CNTL");
      print_GPIO_wake_up();
      break;
    case ESP_SLEEP_WAKEUP_TIMER:
      Serial.println("Wakeup caused by timer");
      break;
    case ESP_SLEEP_WAKEUP_TOUCHPAD:
      Serial.println("Wakeup caused by touchpad");
      break;
    case ESP_SLEEP_WAKEUP_ULP:
      Serial.println("Wakeup caused by ULP program");
      break;
    default:
      Serial.printf("Wakeup was not caused by deep sleep: %d\n", wakeup_reason);
      break;
  }
}

void setup() {
  Serial.begin(115200);
  delay(1000);  //Take some time to open up the Serial Monitor

  //Increment boot number and print it every reboot
  ++bootCount;
  Serial.println("Boot number: " + String(bootCount));

  //Print the wakeup reason for ESP32
  // print_wakeup_reason();
  print_wakeup_button();

  //Use ext1 as a wake-up source
  esp_sleep_enable_ext1_wakeup(bitmask, ESP_EXT1_WAKEUP_ANY_HIGH);
  // enable pull-down resistors and disable pull-up resistors
  // rtc_gpio_pulldown_en(GPIO_NUM_4);
  // rtc_gpio_pullup_dis(GPIO_NUM_4);
  // rtc_gpio_pulldown_en(WAKEUP_GPIO_15);
  // rtc_gpio_pullup_dis(WAKEUP_GPIO_15);
  // pinMode(EXTRA_LED, OUTPUT);
  // digitalWrite(EXTRA_LED, HIGH);
  // delay(1000);
  //Go to sleep now
  Serial.println("Going to sleep now");
  esp_deep_sleep_start();
  Serial.println("This will never be printed");
}



void loop() {
  //This is not going to be called
}