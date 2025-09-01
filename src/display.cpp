#include "display.h"

Arduino_DataBus *bus = new Arduino_ESP32SPI(
  TFT_DC_PIN,
  TFT_CS_PIN,
  TFT_SCK_PIN,
  TFT_MOSI_PIN,
  TFT_MISO_PIN,
  (int32_t)TFT_SPI_HZ
);

Arduino_GFX *gfx = new Arduino_ILI9488_18bit(bus, TFT_RST_PIN, 1);

static uint16_t rgb565(uint8_t r, uint8_t g, uint8_t b) {
  return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
}

static uint16_t tempToColor(float t, float tMin, float tMax) {
  if (!AUTO_SCALE) { tMin = TEMP_MIN_CLAMP; tMax = TEMP_MAX_CLAMP; }
  if (t < tMin) t = tMin;
  if (t > tMax) t = tMax;
  float n = (t - tMin) / (tMax - tMin + 1e-6f);

  uint8_t r = (uint8_t)(255 * n);
  uint8_t g = (uint8_t)(255 * (1 - fabs(0.5f - n) * 2));
  uint8_t b = (uint8_t)(255 * (1 - n));
  return rgb565(r, g, b);
}

void initDisplay() {
  pinMode(TFT_LED_PIN, OUTPUT);        
  digitalWrite(TFT_LED_PIN, HIGH);      
  gfx->begin();                         
  gfx->fillScreen(COL_BG);              
  gfx->setTextColor(COL_TEXT);       
  gfx->setTextSize(2);                  
  gfx->setCursor(10, 10);             
  gfx->println("THEMCAM_PROJECT"); 
}

void drawThermal(const float *buf, float tMin, float tMax) {
  for (int r = 0; r < MLX_H; r++) {          
    for (int c = 0; c < MLX_W; c++) {        
      float t = buf[r * MLX_W + c];        
      uint16_t col = tempToColor(t, tMin, tMax); 
      int x = r * TILE_W;                   
      int y = (MLX_W - 1 - c) * TILE_H;     
      gfx->fillRect(x, y, TILE_W, TILE_H, col);
    }
  }
}

void drawPanel(float tMin, float tMax, bool paused) {
  gfx->fillRect(PANEL_X, 0, PANEL_W, TFT_HEIGHT, COL_BG); 
  gfx->setCursor(PANEL_X + 10, 20);                       
  gfx->setTextColor(paused ? COL_PAUSED : COL_LIVE);      
  gfx->println(paused ? "PAUSED" : "LIVE");              

  gfx->setTextColor(COL_TEXT);                            
  gfx->setCursor(PANEL_X + 10, 60);                       
  gfx->print("Min: "); gfx->print(tMin, 1); gfx->println(" C"); 
  gfx->setCursor(PANEL_X + 10, 100);
  gfx->print("Max: "); gfx->print(tMax, 1); gfx->println(" C"); 
}