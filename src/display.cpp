#include "display.h"
#include <malloc.h>

Arduino_DataBus *bus = new Arduino_ESP32SPI(
  TFT_DC_PIN,
  TFT_CS_PIN,
  TFT_SCK_PIN,
  TFT_MOSI_PIN,
  TFT_MISO_PIN,
  (int32_t)TFT_SPI_HZ
);

Arduino_GFX *gfx = new Arduino_ILI9488_18bit(bus, TFT_RST_PIN, 1);
static uint16_t *frameBuffer = nullptr;
static float smoothedMinTemp = 0.0f;
static float smoothedMaxTemp = 0.0f;
static bool firstTempUpdate = true;
static bool panelInitialized = false;

// ==========================================
// COLOR & CONVERSION UTILITIES
// ==========================================

static inline uint16_t rgb565(uint8_t r, uint8_t g, uint8_t b) {
  return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
}

static uint16_t tempToColor(float t, float tMin, float tMax) {
  if (!AUTO_SCALE) {
    tMin = TEMP_MIN_CLAMP;
    tMax = TEMP_MAX_CLAMP;
  }

  t = constrain(t, tMin, tMax);
  float range = tMax - tMin;
  if (range < 0.1f) range = 0.1f;
  float n = (t - tMin) / range;
  
  uint8_t r, g, b;

  if (n < 0.143f) {
    // black to dark bclue (cold)
    float f = n / 0.143f;
    r = 0;
    g = 0;
    b = (uint8_t)(100 + 155 * f);
  } 
  else if (n < 0.286f) {
    // dark blue to cyan
    float f = (n - 0.143f) / 0.143f;
    r = 0;
    g = (uint8_t)(200 * f);
    b = 255;
  } 
  else if (n < 0.429f) {
    // cyan to green
    float f = (n - 0.286f) / 0.143f;
    r = 0;
    g = 255;
    b = (uint8_t)(255 * (1.0f - f));
  } 
  else if (n < 0.571f) {
    // green to yellow
    float f = (n - 0.429f) / 0.143f;
    r = (uint8_t)(255 * f);
    g = 255;
    b = 0;
  } 
  else if (n < 0.714f) {
    // yellow to orange-red
    float f = (n - 0.571f) / 0.143f;
    r = 255;
    g = (uint8_t)(255 * (1.0f - f * 0.5f));
    b = 0;
  } 
  else if (n < 0.857f) {
    // orange-red to red
    float f = (n - 0.714f) / 0.143f;
    r = 255;
    g = (uint8_t)(128 * (1.0f - f));
    b = 0;
  } 
  else {
    // red to white (hot)
    float f = (n - 0.857f) / 0.143f;
    r = 255;
    g = (uint8_t)(128 * f);
    b = (uint8_t)(128 * f);
  }
  
  return rgb565(r, g, b);
}

// ==========================================
// BILINEAR INTERPOLATION
// ==========================================

static inline float bilinearInterpolate(const float *src, float x, float y) {
  int x0 = (int)x;
  int y0 = (int)y;
  if (x0 >= MLX_W - 1) x0 = MLX_W - 2;
  if (y0 >= MLX_H - 1) y0 = MLX_H - 2;
  if (x0 < 0) x0 = 0;
  if (y0 < 0) y0 = 0;
  
  float fx = x - x0;
  float fy = y - y0;
  
  int idx00 = y0 * MLX_W + x0;
  int idx10 = idx00 + 1;
  int idx01 = idx00 + MLX_W;
  int idx11 = idx01 + 1;
  float v0 = src[idx00] * (1.0f - fx) + src[idx10] * fx;
  float v1 = src[idx01] * (1.0f - fx) + src[idx11] * fx;
  
  return v0 * (1.0f - fy) + v1 * fy;
}

// ==========================================
// INITIALIZATION
// ==========================================

void initDisplay() {
  pinMode(TFT_LED_PIN, OUTPUT);
  digitalWrite(TFT_LED_PIN, HIGH);
  gfx->begin();
  gfx->fillScreen(COL_BG);
  
  delay(100);
 
  if (USE_DMA_ALLOCATION) {
    frameBuffer = (uint16_t*)heap_caps_malloc(
      FB_WIDTH * FB_HEIGHT * sizeof(uint16_t),
      MALLOC_CAP_DMA
    );
  } else {
    frameBuffer = (uint16_t*)malloc(FB_WIDTH * FB_HEIGHT * sizeof(uint16_t));
  }
  
  if (!frameBuffer) {
    Serial.println("framebuffer allocation error");
    gfx->setCursor(10, 50);
    gfx->setTextColor(COL_ACCENT, COL_BG);
    gfx->println("mem 404");
    while (1) delay(1000);
  }
  
  memset(frameBuffer, 0, FB_WIDTH * FB_HEIGHT * sizeof(uint16_t));
  
  Serial.printf("display initialized: %dx%d\n", TFT_WIDTH, TFT_HEIGHT);
  Serial.printf("framebuffer allocated: %dx%d (%d KB)\n",
    FB_WIDTH, FB_HEIGHT,
    (FB_WIDTH * FB_HEIGHT * 2) / 1024
  );
}

void displayStartupScreen() {
  gfx->fillScreen(COL_BG);
  
  gfx->setTextSize(3);
  gfx->setTextColor(COL_LIVE, COL_BG);
  
  int16_t x1, y1;
  uint16_t w, h;
  
  gfx->getTextBounds("Vitovskiy.OS", 0, 0, &x1, &y1, &w, &h);
  int centerX = (FB_WIDTH - w) / 2;
  int centerY = (TFT_HEIGHT - h) / 2;
  
  gfx->setCursor(centerX, centerY);
  gfx->println("Vitovskiy.OS");
  
  gfx->setTextSize(1);
  gfx->setTextColor(COL_TEXT, COL_BG);
  gfx->setCursor(centerX, centerY + 40);
  gfx->println("Thermal Imaging Camera");
  
  delay(2000);
  
  gfx->fillScreen(COL_BG);
}

// ==========================================
// RENDERING
// ==========================================

void drawThermalImage(const float *buf, float tMin, float tMax) {
  if (!buf || !frameBuffer) return;
  if (tMax - tMin < 0.5f) {
    tMax = tMin + 0.5f;
  }
  
  float scaleX = (float)(MLX_W - 1) / FB_WIDTH;
  float scaleY = (float)(MLX_H - 1) / FB_HEIGHT;
  for (int y = 0; y < FB_HEIGHT; y++) {
    float srcY = y * scaleY;
    int rowOffset = y * FB_WIDTH;
    
    for (int x = 0; x < FB_WIDTH; x++) {
      float srcX = x * scaleX;
      float temp = bilinearInterpolate(buf, srcX, srcY);
      frameBuffer[rowOffset + x] = tempToColor(temp, tMin, tMax);
    }
  }

  gfx->draw16bitRGBBitmap(0, 0, frameBuffer, FB_WIDTH, FB_HEIGHT);
}

// ==========================================
// INFO PANEL DRAWING
// ==========================================

void updateInfoPanel(float tMin, float tMax, bool isPaused, float fps) {
  if (firstTempUpdate) {
    smoothedMinTemp = tMin;
    smoothedMaxTemp = tMax;
    firstTempUpdate = false;
  } else {
    smoothedMinTemp = smoothedMinTemp * TEMP_DISPLAY_SMOOTH + 
                      tMin * (1.0f - TEMP_DISPLAY_SMOOTH);
    smoothedMaxTemp = smoothedMaxTemp * TEMP_DISPLAY_SMOOTH + 
                      tMax * (1.0f - TEMP_DISPLAY_SMOOTH);
  }

  if (!panelInitialized) {
    gfx->fillRect(PANEL_X, 0, PANEL_W, TFT_HEIGHT, COL_BG);
    
    gfx->setTextSize(1);
    gfx->setTextColor(COL_TEXT, COL_BG);

    gfx->setCursor(PANEL_X + 6, 60);
    gfx->println("MIN");
    gfx->setCursor(PANEL_X + 6, 90);
    gfx->println("C");
    
    gfx->setCursor(PANEL_X + 6, 130);
    gfx->println("MAX");
    gfx->setCursor(PANEL_X + 6, 160);
    gfx->println("C");
    
    panelInitialized = true;
  }

  gfx->fillRect(PANEL_X + 6, 20, 55, 16, COL_BG);
  gfx->setCursor(PANEL_X + 6, 20);
  gfx->setTextColor(isPaused ? COL_PAUSED : COL_LIVE, COL_BG);
  gfx->setTextSize(1);
  gfx->println(isPaused ? "PAUSE" : "LIVE");
  gfx->fillRect(PANEL_X + 6, 75, 60, 12, COL_BG);
  gfx->setCursor(PANEL_X + 6, 75);
  gfx->setTextColor(COL_TEXT, COL_BG);
  gfx->print(smoothedMinTemp, 1);
  gfx->fillRect(PANEL_X + 6, 145, 60, 12, COL_BG);
  gfx->setCursor(PANEL_X + 6, 145);
  gfx->print(smoothedMaxTemp, 1);

  gfx->fillRect(PANEL_X + 6, 200, 60, 12, COL_BG);
  gfx->setCursor(PANEL_X + 6, 200);
  gfx->setTextColor(0xFFE0, COL_BG);
  gfx->print(fps, 1);
  gfx->println("fps");
  
  int legendX = PANEL_X + 18;
  int legendY = 230;
  int legendW = 30;
  int legendH = 80;
  
  for (int i = 0; i < legendH; i++) {
    float normalized = 1.0f - (float)i / legendH;
    float temp = tMin + normalized * (tMax - tMin);
    uint16_t color = tempToColor(temp, tMin, tMax);
    gfx->drawFastHLine(legendX, legendY + i, legendW, color);
  }
}

void resetDisplayState() {
  firstTempUpdate = true;
  panelInitialized = false;
  smoothedMinTemp = 0.0f;
  smoothedMaxTemp = 0.0f;
}

void setDisplayBrightness(uint8_t level) {
  analogWrite(TFT_LED_PIN, level);
}