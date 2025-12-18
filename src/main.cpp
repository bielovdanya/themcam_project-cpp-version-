//   Code(ver.3.2 , cpp , Arduino Framework) for my ''thermal imaging camera'' project 
//   by Danylo Bielov

//   Based on:
//   - MLX90640: Melexis official datasheet and application notes (infrared thermal sensor array, I2C communication protocol)
//   - Display ILI9488: ILI Technology Corp. datasheet (480x320 TFT LCD controller, SPI interface)
//   - ESP32-S3: Espressif Systems official documentation (ESP-IDF) + (I2C master driver, SPI master driver, GPIO control)
//   - Libraries: Adafruit MLX90640 (official), Arduino_GFX (official)
//   - Framebuffer technique for real-time video display



//======================================================================================================================================//
//                                                                                                                                      //
//   ! Special thanks to Viacheslav Boretskij and Maksym Matsiuk for mentoring and assistance throughout the project development :) !   //
//                                                                                                                                      //
//======================================================================================================================================//



//==============================================================================================================================
//===============================================/    THEMCAM_PROJECT    /======================================================
//==============================================================================================================================


#include <Arduino.h>
#include "main.h"
#include "display.h"
#include "sensor.h"
#include "button.h"

// ==========================================
// GLOBAL STATE
// ==========================================

static float rawFrameBuffer[MLX_W * MLX_H];
static float smoothedFrameBuffer[MLX_W * MLX_H];
static DisplayMode currentMode = MODE_LIVE;
static float lastMinTemp = 0.0f;
static float lastMaxTemp = 0.0f;

static uint32_t frameCounter = 0;
static uint32_t lastStatsTime = 0;
static float currentFPS = 0.0f;
static uint32_t renderTimeAccum = 0;

static uint32_t lastUIUpdate = 0;
static const uint32_t UI_UPDATE_INTERVAL = 100; 

// ==========================================
// OPTIMIZED MIN/MAX 
// ==========================================

void findMinMaxOptimized(const float *buf, float &tMin, float &tMax) {
  tMin = MAX_TEMP_RANGE;
  tMax = MIN_TEMP_INIT;

  int i = 0;
  int limit = (MLX_W * MLX_H) - 3;
  
  for (; i < limit; i += 4) {
    float t0 = buf[i];
    float t1 = buf[i + 1];
    float t2 = buf[i + 2];
    float t3 = buf[i + 3];
    
    if (t0 >= -40.0f && t0 <= 300.0f) {
      if (t0 < tMin) tMin = t0;
      if (t0 > tMax) tMax = t0;
    }
    if (t1 >= -40.0f && t1 <= 300.0f) {
      if (t1 < tMin) tMin = t1;
      if (t1 > tMax) tMax = t1;
    }
    if (t2 >= -40.0f && t2 <= 300.0f) {
      if (t2 < tMin) tMin = t2;
      if (t2 > tMax) tMax = t2;
    }
    if (t3 >= -40.0f && t3 <= 300.0f) {
      if (t3 < tMin) tMin = t3;
      if (t3 > tMax) tMax = t3;
    }
  }

  for (; i < MLX_W * MLX_H; i++) {
    float temp = buf[i];
    if (temp >= -40.0f && temp <= 300.0f) {
      if (temp < tMin) tMin = temp;
      if (temp > tMax) tMax = temp;
    }
  }
}

// ==========================================
// OPTIMIZED SMOOTHING
// ==========================================

void applySmoothingOptimized(float *smoothed, const float *raw) {
  const float alpha = FRAME_SMOOTHING;
  const float beta = 1.0f - FRAME_SMOOTHING;

  int i = 0;
  int limit = (MLX_W * MLX_H) - 3;
  
  for (; i < limit; i += 4) {
    smoothed[i]     = smoothed[i]     * alpha + raw[i]     * beta;
    smoothed[i + 1] = smoothed[i + 1] * alpha + raw[i + 1] * beta;
    smoothed[i + 2] = smoothed[i + 2] * alpha + raw[i + 2] * beta;
    smoothed[i + 3] = smoothed[i + 3] * alpha + raw[i + 3] * beta;
  }

  for (; i < MLX_W * MLX_H; i++) {
    smoothed[i] = smoothed[i] * alpha + raw[i] * beta;
  }
}

// ==========================================
// MODE SWITCHING
// ==========================================

void switchToNextMode() {
  currentMode = (DisplayMode)((currentMode + 1) % MODE_COUNT);
  
  const char* modeNames[] = {"LIVE", "PAUSED", "RGB", "CHARGING"};
  Serial.print("Switched to mode: ");
  Serial.println(modeNames[currentMode]);
  
  if (currentMode == MODE_LIVE) {
    resetDisplayState();
    gfx->fillScreen(COL_BG);
  } else if (currentMode == MODE_CHARGING) {
    gfx->fillRect(FB_X_OFFSET, FB_Y_OFFSET, FB_WIDTH, FB_HEIGHT, rgb565(128, 128, 128));
  }
}

// ==========================================
// SETUP
// ==========================================

void setup() {
  Serial.begin(SERIAL_BAUD);
  delay(SETUP_DELAY_MS);
  
  Serial.println("=================================");
  Serial.println("  THERMAL CAMERA v3.2");
  Serial.println("  by Danylo Bielov");
  Serial.println("=================================");
  
  initDisplay();
  displayStartupScreen();
  initButton();
  
  if (!initSensor()) {
    gfx->fillScreen(COL_BG);
    gfx->setCursor(UI_ERROR_X, UI_ERROR_Y_MAIN);
    gfx->setTextSize(UI_ERROR_TEXT_SIZE);
    gfx->setTextColor(COL_ACCENT);
    gfx->println("SENSOR ERROR");
    gfx->setCursor(UI_ERROR_X, UI_ERROR_Y_SUB);
    gfx->setTextSize(TEXT_SIZE_SMALL);
    gfx->println("Check I2C connection");
    Serial.println("FATAL: Sensor initialization failed!");
    while (1) delay(SENSOR_ERROR_WAIT);
  }

// memset(rawFrameBuffer, 0, sizeof(rawFrameBuffer));
// memset(smoothedFrameBuffer, 0, sizeof(smoothedFrameBuffer));
// gfx->fillScreen(COL_BG);
// resetDisplayState();
// lastStatsTime = millis();
// lastUIUpdate = millis();
}

// ==========================================
// MAIN LOOP
// ==========================================

void loop() {
  buttonUpdate();
  
  if (buttonPressed()) {
    switchToNextMode();
  }
  
  if (currentMode == MODE_CHARGING) {
    drawChargingScreen();
  
    uint32_t now = millis();
    if (now - lastUIUpdate >= UI_UPDATE_INTERVAL) {
      drawMenu(currentMode);
      lastUIUpdate = now;
    }
    
    delay(30);
    return;
  }

  if (currentMode == MODE_LIVE || currentMode == MODE_RGB) {

    if (readFrame(rawFrameBuffer)) {

      applySmoothingOptimized(smoothedFrameBuffer, rawFrameBuffer);
      float tMin, tMax;
      findMinMaxOptimized(smoothedFrameBuffer, tMin, tMax);
      
      if (tMax - tMin < MIN_TEMP_RANGE) {
        tMax = tMin + MIN_TEMP_RANGE;
      }
     
      if (tMax - tMin > 100.0f) {
        float center = (tMin + tMax) / 2.0f;
        tMin = center - 50.0f;
        tMax = center + 50.0f;
      }
      
      lastMinTemp = tMin;
      lastMaxTemp = tMax;
      uint32_t renderStart = micros();
      drawThermalImage(smoothedFrameBuffer, tMin, tMax, currentMode);
      uint32_t renderTime = micros() - renderStart;
    
      uint32_t now = millis();
      if (now - lastUIUpdate >= UI_UPDATE_INTERVAL) {
        drawMenu(currentMode);
        drawLegend(tMin, tMax, currentFPS);
        lastUIUpdate = now;
      }

      frameCounter++;
      renderTimeAccum += renderTime;
      
      if (now - lastStatsTime >= STATS_INTERVAL_MS) {
        float avgRenderMs = renderTimeAccum / (float)frameCounter / MICRO_TO_MS;
        currentFPS = frameCounter * 1000.0f / (now - lastStatsTime);
        
        Serial.printf("Mode: %s | FPS: %.1f | Render: %.2fms | Temp: %.1f-%.1fC\n", 
                      currentMode == MODE_RGB ? "RGB" : "LIVE",
                      currentFPS, avgRenderMs, tMin, tMax);
        
        frameCounter = 0;
        renderTimeAccum = 0;
        lastStatsTime = now;
      }
    }
  } 

  else if (currentMode == MODE_PAUSED) {
    static uint32_t lastUpdate = 0;
    uint32_t now = millis();
    
    if (now - lastUpdate > PAUSE_UPDATE_MS) {
      drawMenu(currentMode);
      drawLegend(lastMinTemp, lastMaxTemp, currentFPS);
      lastUpdate = now;
    }
    
    delay(PAUSE_DELAY_MS);
  }

}
