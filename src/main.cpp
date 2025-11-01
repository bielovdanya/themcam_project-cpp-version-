//   Code(ver.7 , cpp , Arduino Framework) for my ''thermal imaging camera'' project 
//   by Danylo Bielov
//   REAL-TIME rendering using framebuffer approach

//   Based on:
//   - MLX90640: Melexis official datasheet and application notes (infrared thermal sensor array, I2C communication protocol)
//   - Display ILI9488: ILI Technology Corp. datasheet (480x320 TFT LCD controller, SPI interface)
//   - ESP32-S3: Espressif Systems official documentation (ESP-IDF) + (I2C master driver, SPI master driver, GPIO control)
//   - Libraries: Adafruit MLX90640 (official), Arduino_GFX (official)
//   - Framebuffer technique for real-time video display

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

static bool isLive = true;
static float lastMinTemp = 0.0f;
static float lastMaxTemp = 0.0f;

static uint32_t frameCounter = 0;
static uint32_t lastStatsTime = 0;
static float currentFPS = 0.0f;
static uint32_t renderTimeAccum = 0;

static uint32_t lastFrameReadTime = 0;
static const uint32_t MIN_FRAME_TIME_MS = 1000 / SENSOR_FPS;

// ==========================================
// SETUP
// ==========================================

void setup() {
  Serial.begin(SERIAL_BAUD);
  delay(SETUP_DELAY_MS);
  
  initDisplay();
  gfx->fillScreen(COL_BG);
  gfx->setTextSize(TEXT_SIZE_LARGE);
  gfx->setTextColor(COL_LIVE, COL_BG);
  
  int16_t x1, y1;
  uint16_t w, h;
  gfx->getTextBounds("Vitovskiy.OS", 0, 0, &x1, &y1, &w, &h);
  int centerX = (TFT_WIDTH - w) / 2;
  int centerY = (TFT_HEIGHT - h) / 2;
  gfx->setCursor(centerX, centerY - UI_TEXT_SETUP_Y_OFFSET);
  gfx->println("Vitovskiy.OS");
  gfx->setTextSize(TEXT_SIZE_SMALL);
  gfx->setTextColor(COL_TEXT, COL_BG);
  gfx->setCursor(centerX + UI_TEXT_SETUP_X_OFFSET, centerY + UI_TEXT_SETUP_Y_SMALL);
  gfx->println("Thermal Camera by Bielov Danylo");
  
  delay(STARTUP_DELAY_MS + 500);
  gfx->fillScreen(COL_BG);
  initButton();
  if (!initSensor()) {
    gfx->fillScreen(COL_BG);
    gfx->setCursor(UI_ERROR_X, UI_ERROR_Y_MAIN);
    gfx->setTextSize(UI_ERROR_TEXT_SIZE);
    gfx->setTextColor(COL_ACCENT);
    gfx->println("sensor error");
    gfx->setCursor(UI_ERROR_X, UI_ERROR_Y_SUB);
    gfx->setTextSize(TEXT_SIZE_SMALL);
    gfx->println("check I2C connection");
    while (1) delay(SENSOR_ERROR_WAIT);
  }

  memset(rawFrameBuffer, 0, sizeof(rawFrameBuffer));
  memset(smoothedFrameBuffer, 0, sizeof(smoothedFrameBuffer));
  
  gfx->fillScreen(COL_BG);
  resetDisplayState();
  lastStatsTime = millis();
  lastFrameReadTime = millis();
}

// ==========================================
// MAIN LOOP
// ==========================================

void loop() {
  buttonUpdate();
  if (buttonPressed()) {
    isLive = !isLive;
    
    if (!isLive) {
      Serial.println("PAUSED");
    } else {
      Serial.println("LIVE");
      resetDisplayState();
    }
  }
  
  if (isLive) {
    uint32_t now = millis();
    if (now - lastFrameReadTime >= MIN_FRAME_TIME_MS) {
      if (readFrame(rawFrameBuffer)) {
        lastFrameReadTime = now;
        for (int i = 0; i < MLX_W * MLX_H; i++) {
          smoothedFrameBuffer[i] = 
            smoothedFrameBuffer[i] * FRAME_SMOOTHING + 
            rawFrameBuffer[i] * (1.0f - FRAME_SMOOTHING);
        }

        float tMin = MAX_TEMP_RANGE;
        float tMax = MIN_TEMP_INIT;
        
        for (int i = 0; i < MLX_W * MLX_H; i++) {
          float temp = smoothedFrameBuffer[i];
          if (temp < tMin) tMin = temp;
          if (temp > tMax) tMax = temp;
        }
        
        if (tMax - tMin < MIN_TEMP_RANGE) {
          tMax = tMin + MIN_TEMP_RANGE;
        }
        
        lastMinTemp = tMin;
        lastMaxTemp = tMax;
        
        uint32_t renderStart = micros();
        drawThermalImage(smoothedFrameBuffer, tMin, tMax);
        uint32_t renderTime = micros() - renderStart;
        updateInfoPanel(tMin, tMax, false, currentFPS);
        
        frameCounter++;
        renderTimeAccum += renderTime;
        if (now - lastStatsTime >= STATS_INTERVAL_MS) {
          float avgRenderMs = renderTimeAccum / (float)frameCounter / MICRO_TO_MS;
          currentFPS = frameCounter * 1000.0f / (now - lastStatsTime);
          
          Serial.printf("FPS: %.1f | Frames: %d | Render: %.2fms/frame | Temp: %.1f-%.1fC\n", 
                        currentFPS, frameCounter, avgRenderMs, tMin, tMax);
          
          frameCounter = 0;
          renderTimeAccum = 0;
          lastStatsTime = now;
        }
      }
    }
    
  } else {
   
    // PAUSED MODE
    static uint32_t lastUpdate = 0;
    uint32_t now = millis();
    
    if (now - lastUpdate > PAUSE_UPDATE_MS) {
      updateInfoPanel(lastMinTemp, lastMaxTemp, true, currentFPS);
      lastUpdate = now;
    }
    
    delay(PAUSE_DELAY_MS);
  }
}