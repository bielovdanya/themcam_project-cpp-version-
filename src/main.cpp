//   Code(ver.3.1 , cpp , Arduino Framework) for my ''thermal imaging camera'' project 
//   by Danylo Bielov

//   Based on:
//   - MLX90640: Melexis official datasheet and application notes (infrared thermal sensor array, I2C communication protocol)
//   - Display ILI9488: ILI Technology Corp. datasheet (480x320 TFT LCD controller, SPI interface)
//   - ESP32-S3: Espressif Systems official documentation (ESP-IDF) + (I2C master driver, SPI master driver, GPIO control)
//   - Libraries: Adafruit MLX90640 (official), Arduino_GFX (official)
//   - Framebuffer technique for real-time video display



//======================================================================================================================================//
//                                                                                                                                      //
//   ! Special thanks to Viacheslav Boretskij and Maksym Matsyuk for mentoring and assistance throughout the project development :) !   //
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

static uint32_t lastFrameReadTime = 0;
static const uint32_t MIN_FRAME_TIME_MS = 1000 / SENSOR_FPS;

static uint32_t consecutiveErrors = 0;
static const uint32_t MAX_CONSECUTIVE_ERRORS = 5;

// ==========================================
// MODE SWITCHING
// ==========================================

void switchToNextMode() {
  currentMode = (DisplayMode)((currentMode + 1) % MODE_COUNT);
  
  const char* modeNames[] = {"LIVE", "PAUSED", "RGB", "CHARGING"};
  Serial.print("switched to mode: ");
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
  
  initDisplay();
  displayStartupScreen();
  
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
    switchToNextMode();
  }
  
  if (currentMode == MODE_CHARGING) {
    drawChargingScreen();
    drawMenu(currentMode);
    delay(30);
    return;
  }

  if (currentMode == MODE_LIVE || currentMode == MODE_RGB) {
    uint32_t now = millis();
    
    if (now - lastFrameReadTime >= MIN_FRAME_TIME_MS) {
      if (readFrame(rawFrameBuffer)) {
        consecutiveErrors = 0;
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

          if (temp < -40.0f || temp > 300.0f) continue;
          
          if (temp < tMin) tMin = temp;
          if (temp > tMax) tMax = temp;
        }
        

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
        
        drawMenu(currentMode);
        drawLegend(tMin, tMax, currentFPS);

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
      } else {
        consecutiveErrors++;
        if (consecutiveErrors >= MAX_CONSECUTIVE_ERRORS) {
          Serial.println(" errors, attempting recovery ");
          delay(100);
          if (initSensor()) {
            consecutiveErrors = 0;
            Serial.println(" sensor rec successful ");
          } else {
            Serial.println(" sensor rec failed ");
            delay(1000);
          }
        }
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