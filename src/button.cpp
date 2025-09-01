#include "button.h"
#include <Arduino.h>

static int lastState = HIGH;      
static uint32_t lastTime = 0;     

void initButton() {
#if BTN_PULLUP
  pinMode(BTN_PIN, INPUT_PULLUP);
#else
  pinMode(BTN_PIN, INPUT);
#endif
}

bool buttonPressed() {
  int s = digitalRead(BTN_PIN);         
  uint32_t now = millis();               
  if (s != lastState && now - lastTime > BTN_DEBOUNCE_MS)
{ 
    lastState = s; lastTime = now;      
    if (s == LOW) return true;          
}
  return false;                       
}