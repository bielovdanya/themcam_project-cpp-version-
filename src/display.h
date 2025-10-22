#pragma once
#include <Arduino_GFX_Library.h>
#include "main.h"

extern Arduino_GFX *gfx;

void initDisplay();
void displayStartupScreen();
void drawThermalImage(const float *buf, float tMin, float tMax);
void updateInfoPanel(float tMin, float tMax, bool isPaused, float fps);
void resetDisplayState();
void setDisplayBrightness(uint8_t level);