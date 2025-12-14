#include "button.h"
#include <Arduino.h>

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