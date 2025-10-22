#include "sensor.h"
#include <Wire.h>

Adafruit_MLX90640 mlx;
static uint32_t lastFrameTime = 0;
static uint32_t frameCount = 0;
static float avgFPS = 0.0f;
static bool frameReady = false;

bool initSensor() {
  Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN, I2C_FREQ_HZ);
  delay(100);
  if (!mlx.begin(MLX90640_I2C_ADDR, &Wire)) {
    Serial.println("not found");
    return false;
  }
  mlx.setMode(MLX90640_CHESS);           
  mlx.setResolution(MLX90640_ADC_18BIT); 
  mlx.setRefreshRate(MLX90640_16_HZ);    
  
  delay(100);
  
  lastFrameTime = millis();
  frameReady = true;
  
  return true;
}

bool readFrame(float *buf) {
  if (!buf) return false;

  int status = mlx.getFrame(buf);
  
  if (status == 0) {
    uint32_t now = millis();
    uint32_t delta = now - lastFrameTime;
    lastFrameTime = now;
    
    frameCount++;
    if (frameCount >= 16) {
      avgFPS = 1000.0f / ((float)delta);
      frameCount = 0;
    }
    
    frameReady = true;
    return true;
  
} else {
    Serial.printf("sensor read error (status: %d)\n", status);
    frameReady = false;
    return false;
  }
}

bool isFrameReady() {
  return frameReady;
}