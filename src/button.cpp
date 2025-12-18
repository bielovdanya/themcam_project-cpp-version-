#include "button.h"
#include <Arduino.h>


#if USE_INTERRUPTS
static volatile bool pressDetected = false;
static volatile uint32_t lastInterruptTime = 0;

void IRAM_ATTR buttonISR() {
  uint32_t now = millis();
  if (now - lastInterruptTime > BTN_DEBOUNCE_MS) {
    pressDetected = true;
    lastInterruptTime = now;
  }
}

void initButton() {
  pinMode(BTN_PIN, INPUT_PULLUP);
  delay(DISPLAY_INIT_DELAY);
  attachInterrupt(digitalPinToInterrupt(BTN_PIN), buttonISR, FALLING);
}

void buttonUpdate() {

}

bool buttonPressed() {
  if (pressDetected) {
    pressDetected = false;
    return true;
  }
  return false;
}

#else

static int lastStableState = HIGH;
static int currentRawState = HIGH;
static uint32_t lastChangeTime = 0;
static bool pressDetected = false;

void initButton() {
  pinMode(BTN_PIN, INPUT_PULLUP);
  delay(DISPLAY_INIT_DELAY);
  lastStableState = digitalRead(BTN_PIN);
  currentRawState = lastStableState;
  lastChangeTime = millis();
  
}

void buttonUpdate() {
  int rawState = digitalRead(BTN_PIN);
  uint32_t now = millis();
  
  if (rawState != currentRawState) {
    currentRawState = rawState;
    lastChangeTime = now;
    return;  
  }

  if (rawState != lastStableState && (now - lastChangeTime >= BTN_DEBOUNCE_MS)) {
    lastStableState = rawState;
    if (rawState == LOW) {
      pressDetected = true;
    }
  }
}

bool buttonPressed() {
  if (pressDetected) {
    pressDetected = false;
    return true;
  }
  return false;
}

#endif