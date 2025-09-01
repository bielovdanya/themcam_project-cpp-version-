//   Code(ver.1 , cpp , Arduino Framework) for my ''thermal imaging camera'' project 
//   by Danylo Bielov

//   Based on:
//   - MLX90640: Melexis official datasheet and application notes (infrared thermal sensor array, I2C communication protocol)
//   - Display ILI9488: ILI Technology Corp. datasheet (480x320 TFT LCD controller, SPI interface)
//   - ESP32-S3: Espressif Systems official documentation (ESP-IDF) + (I2C master driver, SPI master driver, GPIO control)
//   - Libraries: Adafruit MLX90640 (official), Arduino_GFX (official)

//==============================================================================================================================
//===============================================/    THEMCAM_PROJECT    /======================================================
//==============================================================================================================================

#include <Arduino.h>
#include "main.h"
#include "display.h"
#include "sensor.h"
#include "button.h"

float frameBuf[MLX_W * MLX_H]; 
bool paused = false;          
float lastMin = 0, lastMax = 0;

void setup() {
  initDisplay();             
  initButton();               
  if (!initSensor()) {        
    gfx->setCursor(10, 50);  
    gfx->setTextColor(COL_ACCENT);
    gfx->println("sensor error");
    while (1) delay(1000);  
  }
}

void loop() {
  if (buttonPressed()) paused = !paused; 

  if (!paused) {                          
    if (readFrame(frameBuf)) {         
      float tMin =  999, tMax = -999;    
      for (int i = 0; i < MLX_W * MLX_H; i++) {
        float t = frameBuf[i];           
        if (t < tMin) tMin = t;           
        if (t > tMax) tMax = t;         
      }
      lastMin = tMin; lastMax = tMax;     
      drawThermal(frameBuf, tMin, tMax);  
      drawPanel(tMin, tMax, false);     
    }
  } else {
    drawPanel(lastMin, lastMax, true); 
    delay(50);                            
  }
}