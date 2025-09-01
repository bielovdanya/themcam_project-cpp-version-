#pragma once
#include <Adafruit_MLX90640.h>
#include "main.h"

extern Adafruit_MLX90640 mlx;
bool initSensor();
bool readFrame(float *buf);