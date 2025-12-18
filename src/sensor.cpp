#include "sensor.h"
#include <Wire.h>

Adafruit_MLX90640 mlx;
static uint32_t lastFrameTime = 0;
static uint32_t frameCount = 0;
static float avgFPS = 0.0f;
static bool frameReady = false;

static float lastValidFrame[MLX_W * MLX_H];
static bool lastValidFrameInitialized = false;

#define TEMPORAL_FILTER_SIZE 3
static float temporalBuffer[TEMPORAL_FILTER_SIZE][MLX_W * MLX_H];
static int temporalBufferIndex = 0;
static bool temporalBufferFilled = false;
static float interpolateFromNeighbors(const float *buf, int x, int y) {
  float sum = 0.0f;
  int count = 0;

  const int dx[] = {-1, 1, 0, 0};
  const int dy[] = {0, 0, -1, 1};
  
  for (int i = 0; i < 4; i++) {
    int nx = x + dx[i];
    int ny = y + dy[i];

    if (nx >= 0 && nx < MLX_W && ny >= 0 && ny < MLX_H) {
      int idx = ny * MLX_W + nx;
      float val = buf[idx];

      if (!isnan(val) && val >= -40.0f && val <= 300.0f) {
        sum += val;
        count++;
      }
    }
  }
  
  if (count > 0) {
    return sum / count;
  }
  if (lastValidFrameInitialized) {
    int idx = y * MLX_W + x;
    return lastValidFrame[idx];
  }
  return 25.0f;
}

static inline float getMedian3(float a, float b, float c) {
  if (a > b) {
    if (b > c) return b;      // a > b > c
    if (a > c) return c;      // a > c > b
    return a;                 // c > a > b
  } else {
    if (a > c) return a;      // b > a > c
    if (b > c) return c;      // b > c > a
    return b;                 // c > b > a
  }
}

bool initSensor() {
  Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN, I2C_FREQ_HZ);
  delay(SENSOR_INIT_DELAY);
  
  if (!mlx.begin(MLX90640_I2C_ADDR, &Wire)) {
    Serial.println("mlx not found");
    return false;
  }
  
  mlx.setMode(MLX90640_CHESS);           
  mlx.setResolution(MLX90640_ADC_18BIT); 
  mlx.setRefreshRate(MLX90640_16_HZ);      
  delay(SENSOR_INIT_DELAY);
  memset(temporalBuffer, 0, sizeof(temporalBuffer));
  temporalBufferIndex = 0;
  temporalBufferFilled = false;
  
  lastFrameTime = millis();
  frameReady = true;

  return true;
}

bool readFrame(float *buf) {
  if (!buf) return false;

  int status = mlx.getFrame(buf);
  
  if (status != 0) {
    Serial.printf("sensor read error (status: %d)\n", status);
    frameReady = false;
    return false;
  }
  
  memcpy(temporalBuffer[temporalBufferIndex], buf, MLX_W * MLX_H * sizeof(float));
  temporalBufferIndex = (temporalBufferIndex + 1) % TEMPORAL_FILTER_SIZE;
  
  if (temporalBufferIndex == 0) {
    temporalBufferFilled = true;
  }
  
  if (temporalBufferFilled) {
    for (int i = 0; i < MLX_W * MLX_H; i++) {
      buf[i] = getMedian3(
        temporalBuffer[0][i],
        temporalBuffer[1][i],
        temporalBuffer[2][i]
      );
    }
  }
  
  int invalidCount = 0;
  bool pixelFixed[MLX_W * MLX_H] = {false};

  for (int i = 0; i < MLX_W * MLX_H; i++) {
    if (isnan(buf[i]) || buf[i] < -40.0f || buf[i] > 300.0f) {
      pixelFixed[i] = true;
      invalidCount++;
    }
  }

  if (invalidCount > (MLX_W * MLX_H / 4)) {
    Serial.printf("frame rejected: %d/%d crptd pixels (%.1f%%)\n", 
                  invalidCount, MLX_W * MLX_H, 
                  100.0f * invalidCount / (MLX_W * MLX_H));
    frameReady = false;
    return false;
  }
  
  if (invalidCount > 0) {
    for (int y = 0; y < MLX_H; y++) {
      for (int x = 0; x < MLX_W; x++) {
        int idx = y * MLX_W + x;
        if (pixelFixed[idx]) {
          buf[idx] = interpolateFromNeighbors(buf, x, y);
        }
      }
    }
  }

  memcpy(lastValidFrame, buf, MLX_W * MLX_H * sizeof(float));
  lastValidFrameInitialized = true;
  uint32_t now = millis();
  uint32_t delta = now - lastFrameTime;
  lastFrameTime = now;
  
  frameCount++;
  if (frameCount >= FRAME_SMOOTH_COUNT) {
    if (delta > 0) {
      avgFPS = (float)MS_TO_MICRO / ((float)delta);
    }
    frameCount = 0;
  }
  
  frameReady = true;
  return true;
}

bool isFrameReady() {
  return frameReady;
}