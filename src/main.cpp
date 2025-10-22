//   Code(ver.6 , cpp , Arduino Framework) for my ''thermal imaging camera'' project 
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
  Serial.begin(115200);
  delay(500);
  
  Serial.println("\n\n========================================");
  Serial.println("   VITOVSKIY.OS");
  Serial.println("   Real-Time Imaging System");
  Serial.println("========================================\n");
  initDisplay();
  gfx->fillScreen(COL_BG);
  gfx->setTextSize(4);
  gfx->setTextColor(COL_LIVE, COL_BG);
  
  int16_t x1, y1;
  uint16_t w, h;
  gfx->getTextBounds("Vitovskiy.OS", 0, 0, &x1, &y1, &w, &h);
  int centerX = (TFT_WIDTH - w) / 2;
  int centerY = (TFT_HEIGHT - h) / 2;
  gfx->setCursor(centerX, centerY - 30);
  gfx->println("Vitovskiy.OS");
  gfx->setTextSize(1);
  gfx->setTextColor(COL_TEXT, COL_BG);
  gfx->setCursor(centerX + 20, centerY + 40);
  gfx->println("Thermal Camera");
  
  delay(2500);
  gfx->fillScreen(COL_BG);
  initButton();
  if (!initSensor()) {
    gfx->fillScreen(COL_BG);
    gfx->setCursor(20, 80);
    gfx->setTextSize(2);
    gfx->setTextColor(COL_ACCENT);
    gfx->println("sensor error");
    gfx->setCursor(20, 110);
    gfx->setTextSize(1);
    gfx->println("check I2C connection");
    while (1) delay(1000);
  }

  memset(rawFrameBuffer, 0, sizeof(rawFrameBuffer));
  memset(smoothedFrameBuffer, 0, sizeof(smoothedFrameBuffer));
  
  gfx->fillScreen(COL_BG);
  resetDisplayState();
  
  Serial.println("\n╔══════════════════════════════════════╗");
  Serial.println("║   system ready                       ║");
  Serial.println("╚══════════════════════════════════════╝\n");
  
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
      Serial.println("⏸ paused ");
    } else {
      Serial.println("▶ live ");
      resetDisplayState();
    }
  }
  
  // ==========================================
  // LIVE MODE
  // ==========================================
  
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
        float tMin = 999.0f;
        float tMax = -999.0f;
        
        for (int i = 0; i < MLX_W * MLX_H; i++) {
          float temp = smoothedFrameBuffer[i];
          if (temp < tMin) tMin = temp;
          if (temp > tMax) tMax = temp;
        }
        if (tMax - tMin < 0.5f) {
          tMax = tMin + 0.5f;
        }
        
        lastMinTemp = tMin;
        lastMaxTemp = tMax;
        uint32_t renderStart = micros();
        drawThermalImage(smoothedFrameBuffer, tMin, tMax);
        uint32_t renderTime = micros() - renderStart;
        updateInfoPanel(tMin, tMax, false, currentFPS);
        frameCounter++;
        renderTimeAccum += renderTime;
        if (now - lastStatsTime >= 2000) {
          float avgRenderMs = renderTimeAccum / (float)frameCounter / 1000.0f;
          currentFPS = frameCounter * 1000.0f / (now - lastStatsTime);
          
          Serial.printf("FPS: %.1f | Render: %.2fms | Temp: %.1f-%.1f°C\n", 
                        currentFPS, avgRenderMs, tMin, tMax);
          
          frameCounter = 0;
          renderTimeAccum = 0;
          lastStatsTime = now;
        }
      }
    }
    
  } 
  // ==========================================
  // PAUSED MODE
  // ==========================================
  
  else {
    static uint32_t lastUpdate = 0;
    uint32_t now = millis();
    
    if (now - lastUpdate > 100) {
      updateInfoPanel(lastMinTemp, lastMaxTemp, true, currentFPS);
      lastUpdate = now;
    }
    
    delay(10); 
  }
}