#pragma once
#include <Arduino_GFX_Library.h>
#include "main.h"

extern Arduino_GFX *gfx;
void initDisplay();
void drawThermal(const float *buf, float tMin, float tMax);
void drawPanel(float tMin, float tMax, bool paused);