#pragma once
#include <Arduino_GFX_Library.h>
#include "main.h"

extern Arduino_GFX *gfx;
uint16_t rgb565(uint8_t r, uint8_t g, uint8_t b);

void initDisplay();
void displayStartupScreen();
void drawThermalImage(const float *buf, float tMin, float tMax, DisplayMode mode);
void drawMenu(DisplayMode currentMode);
void drawLegend(float tMin, float tMax, float fps);
void drawChargingScreen();
void resetDisplayState();
void setDisplayBrightness(uint8_t level);