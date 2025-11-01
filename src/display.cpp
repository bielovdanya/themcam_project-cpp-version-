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
static float prevMinTemp = 0.0f;
static float prevMaxTemp = 0.0f;
static bool firstTempUpdate = true;
static bool panelInitialized = false;
static bool scalingChanged = false;

// ==========================================
// COLOR/CONVERSION UTILITIES
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
  if (range < MIN_RANGE_DEFAULT) range = MIN_RANGE_DEFAULT;
  float n = (t - tMin) / range;
  
  uint8_t r, g, b;

  if (n < COLOR_STEP_SIZE) {
    float f = n / COLOR_STEP_SIZE;
    r = 0;
    g = 0;
    b = (uint8_t)(CMAP_BLUE_MIN + CMAP_BLUE_MAX * f - CMAP_BLUE_MIN * f);
  } 
  else if (n < COLOR_STEP_SIZE * 2) {
    float f = (n - COLOR_STEP_SIZE) / COLOR_STEP_SIZE;
    r = 0;
    g = (uint8_t)(CMAP_GREEN_MID * f);
    b = CMAP_BLUE_MAX;
  } 
  else if (n < COLOR_STEP_SIZE * 3) {
    float f = (n - COLOR_STEP_SIZE * 2) / COLOR_STEP_SIZE;
    r = 0;
    g = CMAP_GREEN_MAX;
    b = (uint8_t)(CMAP_BLUE_MAX * (1.0f - f));
  } 
  else if (n < COLOR_STEP_SIZE * 4) {
    float f = (n - COLOR_STEP_SIZE * 3) / COLOR_STEP_SIZE;
    r = (uint8_t)(255 * f);
    g = CMAP_GREEN_MAX;
    b = 0;
  } 
  else if (n < COLOR_STEP_SIZE * 5) {
    float f = (n - COLOR_STEP_SIZE * 4) / COLOR_STEP_SIZE;
    r = 255;
    g = (uint8_t)(CMAP_GREEN_MAX * (1.0f - f * CMAP_RED_HALF));
    b = 0;
  } 
  else if (n < COLOR_STEP_SIZE * 6) {
    float f = (n - COLOR_STEP_SIZE * 5) / COLOR_STEP_SIZE;
    r = 255;
    g = (uint8_t)(CMAP_RED_MID * (1.0f - f));
    b = 0;
  } 
  else {
    float f = (n - COLOR_STEP_SIZE * 6) / COLOR_STEP_SIZE;
    r = 255;
    g = (uint8_t)(CMAP_RED_MID * f);
    b = (uint8_t)(CMAP_RED_MID * f);
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
  
  delay(DISPLAY_INIT_DELAY);
 
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
    gfx->setCursor(UI_CURSOR_X, UI_CURSOR_Y);
    gfx->setTextColor(COL_ACCENT, COL_BG);
    gfx->println("mem 404");
    Serial.printf("tried to allocate: %d bytes\n", FB_WIDTH * FB_HEIGHT * sizeof(uint16_t));
    Serial.printf("free heap: %d bytes\n", heap_caps_get_free_size(MALLOC_CAP_8BIT));
    while (1) delay(SENSOR_ERROR_WAIT);
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
  
  gfx->setTextSize(TEXT_SIZE_LARGE);
  gfx->setTextColor(COL_LIVE, COL_BG);
  
  int16_t x1, y1;
  uint16_t w, h;
  
  gfx->getTextBounds("Vitovskiy.OS", 0, 0, &x1, &y1, &w, &h);
  int centerX = (FB_WIDTH - w) / 2;
  int centerY = (TFT_HEIGHT - h) / 2;
  
  gfx->setCursor(centerX, centerY);
  gfx->println("Vitovskiy.OS");
  
  gfx->setTextSize(TEXT_SIZE_SMALL);
  gfx->setTextColor(COL_TEXT, COL_BG);
  gfx->setCursor(centerX, centerY + UI_TEXT_SETUP_Y_SMALL);
  gfx->println("Thermal Imaging Camera");
  
  delay(STARTUP_DELAY_MS);
  
  gfx->fillScreen(COL_BG);
}

// ==========================================
// RENDERING
// ==========================================

void drawThermalImage(const float *buf, float tMin, float tMax) {
  if (!buf || !frameBuffer) return;
  if (tMax - tMin < MIN_TEMP_RANGE) {
    tMax = tMin + MIN_TEMP_RANGE;
  }
  
  bool scalingJustChanged = false;
  if (tMin != prevMinTemp || tMax != prevMaxTemp) {
    scalingChanged = true;
    scalingJustChanged = true;
  }
  
  float scaleX = (float)(MLX_W - 1) / FB_WIDTH;
  float scaleY = (float)(MLX_H - 1) / FB_HEIGHT;
  
  int changedPixels = 0;
  for (int y = 0; y < FB_HEIGHT; y++) {
    float srcY = y * scaleY;
    int rowOffset = y * FB_WIDTH;
    
    for (int x = 0; x < FB_WIDTH; x++) {
      float srcX = x * scaleX;
      float temp = bilinearInterpolate(buf, srcX, srcY);
      uint16_t newColor = tempToColor(temp, tMin, tMax);
      uint16_t oldColor = frameBuffer[rowOffset + x];
      
      frameBuffer[rowOffset + x] = newColor;
      if (newColor != oldColor || scalingJustChanged) {
        changedPixels++;
      }
    }
  }
  
  gfx->draw16bitRGBBitmap(0, 0, frameBuffer, FB_WIDTH, FB_HEIGHT);
  
  prevMinTemp = tMin;
  prevMaxTemp = tMax;
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
    
    gfx->setTextSize(PANEL_LABEL_SIZE);
    gfx->setTextColor(COL_TEXT, COL_BG);

    gfx->setCursor(PANEL_X + PANEL_X_OFFSET, PANEL_Y_MIN_LABEL);
    gfx->println("MIN");
    gfx->setCursor(PANEL_X + PANEL_X_OFFSET, PANEL_Y_MIN_UNIT);
    gfx->println("C");
    
    gfx->setCursor(PANEL_X + PANEL_X_OFFSET, PANEL_Y_MAX_LABEL);
    gfx->println("MAX");
    gfx->setCursor(PANEL_X + PANEL_X_OFFSET, PANEL_Y_MAX_UNIT);
    gfx->println("C");
    
    panelInitialized = true;
  }

  gfx->fillRect(PANEL_X + PANEL_X_OFFSET, PANEL_Y_LIVE_STATUS, PANEL_MIN_VAL_W, PANEL_MIN_VAL_H, COL_BG);
  gfx->setCursor(PANEL_X + PANEL_X_OFFSET, PANEL_Y_LIVE_STATUS);
  gfx->setTextColor(isPaused ? COL_PAUSED : COL_LIVE, COL_BG);
  gfx->setTextSize(PANEL_LABEL_SIZE);
  gfx->println(isPaused ? "PAUSE" : "LIVE");
  
  gfx->fillRect(PANEL_X + PANEL_X_OFFSET, PANEL_Y_MIN_VAL, PANEL_MIN_VAL_W, PANEL_MIN_VAL_H, COL_BG);
  gfx->setCursor(PANEL_X + PANEL_X_OFFSET, PANEL_Y_MIN_VAL);
  gfx->setTextColor(COL_TEXT, COL_BG);
  gfx->print(smoothedMinTemp, TEMP_PRECISION);
  
  gfx->fillRect(PANEL_X + PANEL_X_OFFSET, PANEL_Y_MAX_VAL, PANEL_MIN_VAL_W, PANEL_MIN_VAL_H, COL_BG);
  gfx->setCursor(PANEL_X + PANEL_X_OFFSET, PANEL_Y_MAX_VAL);
  gfx->print(smoothedMaxTemp, TEMP_PRECISION);

  gfx->fillRect(PANEL_X + PANEL_X_OFFSET, PANEL_Y_FPS, PANEL_MIN_VAL_W, PANEL_MIN_VAL_H, COL_BG);
  gfx->setCursor(PANEL_X + PANEL_X_OFFSET, PANEL_Y_FPS);
  gfx->setTextColor(COL_RGB_TEXT, COL_BG);
  gfx->print(fps, FPS_PRECISION);
  gfx->println("fps");
  
  int legendX = PANEL_X + PANEL_X_OFFSET + 12;
  int legendY = PANEL_Y_LEGEND;
  
  for (int i = 0; i < LEGEND_HEIGHT; i++) {
    float normalized = 1.0f - (float)i / LEGEND_HEIGHT;
    float temp = tMin + normalized * (tMax - tMin);
    uint16_t color = tempToColor(temp, tMin, tMax);
    gfx->drawFastHLine(legendX, legendY + i, LEGEND_WIDTH, color);
  }
}

void resetDisplayState() {
  firstTempUpdate = true;
  panelInitialized = false;
  smoothedMinTemp = 0.0f;
  smoothedMaxTemp = 0.0f;
  scalingChanged = false;
}

void setDisplayBrightness(uint8_t level) {
  analogWrite(TFT_LED_PIN, level);
}