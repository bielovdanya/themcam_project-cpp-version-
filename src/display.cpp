#include "display.h"
#include <malloc.h>

Arduino_DataBus *bus = new Arduino_ESP32SPI(
  TFT_DC_PIN, TFT_CS_PIN, TFT_SCK_PIN, TFT_MOSI_PIN, TFT_MISO_PIN, (int32_t)TFT_SPI_HZ
);

Arduino_GFX *gfx = new Arduino_ILI9488_18bit(bus, TFT_RST_PIN, 1);
static uint16_t *frameBuffer = nullptr;
static float *gradientBuffer = nullptr;
static float smoothedMinTemp = 0.0f;
static float smoothedMaxTemp = 0.0f;
static bool firstTempUpdate = true;
static bool menuInitialized = false;
static bool legendInitialized = false;
static int minTempX = 0;
static int minTempY = 0;
static int maxTempX = 0;
static int maxTempY = 0;
static float menuItemAlpha[MODE_COUNT] = {1.0f, 0.3f, 0.3f, 0.3f};
static float menuItemTargetAlpha[MODE_COUNT] = {1.0f, 0.3f, 0.3f, 0.3f};
static const float MENU_ANIM_SPEED = 0.15f;

static uint16_t colorLUT[COLOR_LUT_SIZE];
static uint16_t colorLUT_RGB[COLOR_LUT_SIZE];
static bool lutInitialized = false;

// ==========================================
// COLOR LUT INITIALIZATION
// ==========================================

static void initColorLUT() {
  if (lutInitialized) return;
  
  for (int i = 0; i < COLOR_LUT_SIZE; i++) {
    float n = (float)i / (COLOR_LUT_SIZE - 1);
    uint8_t r, g, b;
    
    if (n < 0.15f) {
      float f = n / 0.15f;
      r = (uint8_t)(50 * f);
      g = 0;
      b = (uint8_t)(80 + 175 * f);
    }
    else if (n < 0.30f) {
      float f = (n - 0.15f) / 0.15f;
      r = (uint8_t)(50 + 155 * f);
      g = 0;
      b = 255;
    }
    else if (n < 0.45f) {
      float f = (n - 0.30f) / 0.15f;
      r = 205 + (uint8_t)(50 * f);
      g = 0;
      b = (uint8_t)(255 * (1.0f - f));
    }
    else if (n < 0.60f) {
      float f = (n - 0.45f) / 0.15f;
      r = 255;
      g = (uint8_t)(80 * f);
      b = 0;
    }
    else if (n < 0.75f) {
      float f = (n - 0.60f) / 0.15f;
      r = 255;
      g = (uint8_t)(80 + 175 * f);
      b = 0;
    }
    else if (n < 0.90f) {
      float f = (n - 0.75f) / 0.15f;
      r = 255;
      g = 255;
      b = (uint8_t)(255 * f);
    }
    else {
      r = 255;
      g = 255;
      b = 255;
    }
    
    colorLUT[i] = ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);

    if (n < 0.25f) {
      float f = n / 0.25f;
      r = 0;
      g = (uint8_t)(255 * f);
      b = 255;
    }
    else if (n < 0.5f) {
      float f = (n - 0.25f) / 0.25f;
      r = 0;
      g = 255;
      b = (uint8_t)(255 * (1.0f - f));
    }
    else if (n < 0.75f) {
      float f = (n - 0.5f) / 0.25f;
      r = (uint8_t)(255 * f);
      g = 255;
      b = 0;
    }
    else {
      float f = (n - 0.75f) / 0.25f;
      r = 255;
      g = (uint8_t)(255 * (1.0f - f));
      b = 0;
    }
    
    colorLUT_RGB[i] = ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
  }
  
  lutInitialized = true;
}

// ==========================================
// FAST COLOR LOOKUP
// ==========================================

static inline uint16_t tempToColorFast(float t, float tMin, float tMax, bool useRGB) {
  if (!AUTO_SCALE) {
    tMin = TEMP_MIN_CLAMP;
    tMax = TEMP_MAX_CLAMP;
  }

  t = constrain(t, tMin, tMax);
  float range = tMax - tMin;
  if (range < MIN_RANGE_DEFAULT) range = MIN_RANGE_DEFAULT;
  float n = (t - tMin) / range;
  
  int idx = (int)(n * (COLOR_LUT_SIZE - 1));
  idx = constrain(idx, 0, COLOR_LUT_SIZE - 1);
  
  return useRGB ? colorLUT_RGB[idx] : colorLUT[idx];
}

uint16_t rgb565(uint8_t r, uint8_t g, uint8_t b) {
  return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
}

static inline uint16_t blendColor(uint16_t c1, uint16_t c2, float alpha) {
  uint8_t r1 = (c1 >> 11) << 3;
  uint8_t g1 = ((c1 >> 5) & 0x3F) << 2;
  uint8_t b1 = (c1 & 0x1F) << 3;
  
  uint8_t r2 = (c2 >> 11) << 3;
  uint8_t g2 = ((c2 >> 5) & 0x3F) << 2;
  uint8_t b2 = (c2 & 0x1F) << 3;
  
  uint8_t r = r1 + (r2 - r1) * alpha;
  uint8_t g = g1 + (g2 - g1) * alpha;
  uint8_t b = b1 + (b2 - b1) * alpha;
  
  return rgb565(r, g, b);
}

// ==========================================
// OPTIMIZED GRADIENT CALCULATION
// ==========================================

static void calculateGradients(const float *tempBuf) {
  if (!EDGE_DETECTION_ENABLED || !gradientBuffer) return;
  
  const uint32_t FIXED_SHIFT = 16;
  const uint32_t scaleX_fixed = ((MLX_W - 1) << FIXED_SHIFT) / FB_WIDTH;
  const uint32_t scaleY_fixed = ((MLX_H - 1) << FIXED_SHIFT) / FB_HEIGHT;
  
  for (int x = 0; x < FB_WIDTH; x++) {
    gradientBuffer[x] = 0.0f;
    gradientBuffer[(FB_HEIGHT - 1) * FB_WIDTH + x] = 0.0f;
  }
  for (int y = 1; y < FB_HEIGHT - 1; y++) {
    gradientBuffer[y * FB_WIDTH] = 0.0f;
    gradientBuffer[y * FB_WIDTH + FB_WIDTH - 1] = 0.0f;
  }
  
  uint32_t currentY = scaleY_fixed;
  
  for (int y = 1; y < FB_HEIGHT - 1; y++) {
    int y0 = currentY >> FIXED_SHIFT;
    if (y0 >= MLX_H - 2) y0 = MLX_H - 3;
    
    float fy = (float)(currentY & 0xFFFF) / 65536.0f;
    
    const float *row_m1 = &tempBuf[(y0 - 1) * MLX_W];
    const float *row_0 = &tempBuf[y0 * MLX_W];
    const float *row_p1 = &tempBuf[(y0 + 1) * MLX_W];
    const float *row_p2 = &tempBuf[(y0 + 2) * MLX_W];
    
    int rowOffset = y * FB_WIDTH;
    uint32_t currentX = scaleX_fixed;
    
    for (int x = 1; x < FB_WIDTH - 1; x++) {
      int x0 = currentX >> FIXED_SHIFT;
      if (x0 >= MLX_W - 2) x0 = MLX_W - 3;
      
      float fx = (float)(currentX & 0xFFFF) / 65536.0f;
      
      float tl = row_m1[x0] + (row_m1[x0 + 1] - row_m1[x0]) * fx;
      float tc = row_m1[x0] + (row_m1[x0 + 1] - row_m1[x0]) * fx;
      float tr = row_m1[x0 + 1] + (row_m1[x0 + 2] - row_m1[x0 + 1]) * fx;
      
      float ml = row_0[x0 - 1] + (row_0[x0] - row_0[x0 - 1]) * fx;
      float mr = row_0[x0 + 1] + (row_0[x0 + 2] - row_0[x0 + 1]) * fx;
      
      float bl = row_p1[x0] + (row_p1[x0 + 1] - row_p1[x0]) * fx;
      float bc = row_p1[x0] + (row_p1[x0 + 1] - row_p1[x0]) * fx;
      float br = row_p1[x0 + 1] + (row_p1[x0 + 2] - row_p1[x0 + 1]) * fx;
      
      float gx = -tl + tr - 2.0f * ml + 2.0f * mr - bl + br;
      float gy = -tl - 2.0f * tc - tr + bl + 2.0f * bc + br;
      
      gradientBuffer[rowOffset + x] = sqrtf(gx * gx + gy * gy) * 0.125f;
      
      currentX += scaleX_fixed;
    }
    
    currentY += scaleY_fixed;
  }
}

static void drawTempMarkers(float minTemp, float maxTemp);

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
      FB_WIDTH * FB_HEIGHT * sizeof(uint16_t), MALLOC_CAP_DMA
    );
  } else {
    frameBuffer = (uint16_t*)malloc(FB_WIDTH * FB_HEIGHT * sizeof(uint16_t));
  }
  
  if (!frameBuffer) {
    Serial.println("framebuffer allocation error");
    while (1) delay(SENSOR_ERROR_WAIT);
  }
  
  if (EDGE_DETECTION_ENABLED) {
    if (USE_DMA_ALLOCATION) {
      gradientBuffer = (float*)heap_caps_malloc(
        FB_WIDTH * FB_HEIGHT * sizeof(float), MALLOC_CAP_DMA
      );
    } else {
      gradientBuffer = (float*)malloc(FB_WIDTH * FB_HEIGHT * sizeof(float));
    }
    
    if (gradientBuffer) {
      memset(gradientBuffer, 0, FB_WIDTH * FB_HEIGHT * sizeof(float));
    }
  }
  
  memset(frameBuffer, 0, FB_WIDTH * FB_HEIGHT * sizeof(uint16_t));
  
  initColorLUT();
  
  Serial.printf("display initialized: %dx%d\n", TFT_WIDTH, TFT_HEIGHT);
  Serial.printf("framebuffer: %dx%d (%d KB)\n", FB_WIDTH, FB_HEIGHT, (FB_WIDTH * FB_HEIGHT * 2) / 1024);
  Serial.println("color LUT initialized (256 entries)");
}

void displayStartupScreen() {
  gfx->fillScreen(COL_BG);
  
  gfx->setTextSize(TEXT_SIZE_LARGE);
  gfx->setTextColor(COL_LIVE, COL_BG);
  
  int16_t x1, y1;
  uint16_t w, h;
  
  gfx->getTextBounds("Vitovskiy.OS", 0, 0, &x1, &y1, &w, &h);
  int centerX = (TFT_WIDTH - w) / 2;
  int centerY = (TFT_HEIGHT - h) / 2;
  
  gfx->setCursor(centerX, centerY + 5);
  gfx->println("Vitovskiy.OS");
  
  gfx->setTextSize(TEXT_SIZE_SMALL);
  gfx->setTextColor(COL_TEXT, COL_BG);
  gfx->setCursor(centerX + 15 , centerY + 45);
  gfx->println("Thermal Camera by Bielov Danylo");
  
  delay(STARTUP_DELAY_MS);
  gfx->fillScreen(COL_BG);
}

// ==========================================
// OPTIMIZED RENDERING WITH FIXED-POINT
// ==========================================

void drawThermalImage(const float *buf, float tMin, float tMax, DisplayMode mode) {
  if (!buf || !frameBuffer) return;
  
  if (tMax - tMin < MIN_TEMP_RANGE) {
    tMax = tMin + MIN_TEMP_RANGE;
  }

  if ((mode == MODE_LIVE || mode == MODE_PAUSED) && EDGE_DETECTION_ENABLED) {
    calculateGradients(buf);
  }

  float localMin = 999.0f;
  float localMax = -999.0f;
  int minIdx = 0, maxIdx = 0;
  
  for (int i = 0; i < MLX_W * MLX_H; i++) {
    float val = buf[i];
    if (val < localMin) { localMin = val; minIdx = i; }
    if (val > localMax) { localMax = val; maxIdx = i; }
  }

  int minSensorX = minIdx % MLX_W;
  int minSensorY = minIdx / MLX_W;
  int maxSensorX = maxIdx % MLX_W;
  int maxSensorY = maxIdx / MLX_W;
  
  minTempX = FB_X_OFFSET + (minSensorX * FB_WIDTH) / (MLX_W - 1);
  minTempY = FB_Y_OFFSET + (minSensorY * FB_HEIGHT) / (MLX_H - 1);
  maxTempX = FB_X_OFFSET + (maxSensorX * FB_WIDTH) / (MLX_W - 1);
  maxTempY = FB_Y_OFFSET + (maxSensorY * FB_HEIGHT) / (MLX_H - 1);

  const uint32_t FIXED_SHIFT = 16;
  const uint32_t scaleX_fixed = ((MLX_W - 1) << FIXED_SHIFT) / FB_WIDTH;
  const uint32_t scaleY_fixed = ((MLX_H - 1) << FIXED_SHIFT) / FB_HEIGHT;
  
  const bool useRGB = (mode == MODE_RGB);
  const bool useEdges = (EDGE_DETECTION_ENABLED && gradientBuffer && !useRGB);
  const uint16_t edgeColor = 0xFFFF;

  uint32_t currentY_fixed = 0;

  for (int y = 0; y < FB_HEIGHT; y++) {
    int y0 = currentY_fixed >> FIXED_SHIFT;
    if (y0 >= MLX_H - 1) y0 = MLX_H - 2;
    
    float fy = (float)(currentY_fixed & 0xFFFF) / 65536.0f;

    const float *row0 = &buf[y0 * MLX_W];
    const float *row1 = &buf[(y0 + 1) * MLX_W];

    int rowOffset = y * FB_WIDTH;
    uint32_t currentX_fixed = 0;

    for (int x = 0; x < FB_WIDTH; x++) {
      int x0 = currentX_fixed >> FIXED_SHIFT;
      if (x0 >= MLX_W - 1) x0 = MLX_W - 2;

      float fx = (float)(currentX_fixed & 0xFFFF) / 65536.0f;
      float t00 = row0[x0];
      float t10 = row0[x0 + 1];
      float t01 = row1[x0];
      float t11 = row1[x0 + 1];
      float top = t00 + (t10 - t00) * fx;
      float bot = t01 + (t11 - t01) * fx;
      float temp = top + (bot - top) * fy;
      uint16_t color = tempToColorFast(temp, tMin, tMax, useRGB);

      if (useEdges && gradientBuffer[rowOffset + x] > EDGE_THRESHOLD) {
        color = edgeColor;
      }
      
      frameBuffer[rowOffset + x] = color;

      currentX_fixed += scaleX_fixed;
    }

    currentY_fixed += scaleY_fixed;
  }

  gfx->draw16bitRGBBitmap(FB_X_OFFSET, FB_Y_OFFSET, frameBuffer, FB_WIDTH, FB_HEIGHT);
  
  drawTempMarkers(localMin, localMax);
}

static void drawTempMarkers(float minTemp, float maxTemp) {
  const int markerSize = 6;
  const int textOffset = 10;
  
  uint16_t maxColor = rgb565(255, 50, 50);
  gfx->drawLine(maxTempX - markerSize, maxTempY - markerSize, 
                maxTempX + markerSize, maxTempY + markerSize, maxColor);
  gfx->drawLine(maxTempX + markerSize, maxTempY - markerSize, 
                maxTempX - markerSize, maxTempY + markerSize, maxColor);
  
  gfx->setTextSize(1);
  gfx->setTextColor(maxColor, COL_BG);
  char maxStr[8];
  snprintf(maxStr, sizeof(maxStr), "%.1f", maxTemp);
  
  int16_t x1, y1;
  uint16_t w, h;
  gfx->getTextBounds(maxStr, 0, 0, &x1, &y1, &w, &h);
  int maxTextX = maxTempX - w / 2;
  int maxTextY = maxTempY + textOffset;
  
  if (maxTextX < FB_X_OFFSET) maxTextX = FB_X_OFFSET;
  if (maxTextX + w > FB_X_OFFSET + FB_WIDTH) maxTextX = FB_X_OFFSET + FB_WIDTH - w;
  if (maxTextY + h > FB_Y_OFFSET + FB_HEIGHT) maxTextY = maxTempY - textOffset - h;
  
  gfx->setCursor(maxTextX, maxTextY);
  gfx->print(maxStr);
  
  uint16_t minColor = rgb565(100, 200, 255);
  gfx->drawLine(minTempX - markerSize, minTempY - markerSize, 
                minTempX + markerSize, minTempY + markerSize, minColor);
  gfx->drawLine(minTempX + markerSize, minTempY - markerSize, 
                minTempX - markerSize, minTempY + markerSize, minColor);
  
  gfx->setTextColor(minColor, COL_BG);
  char minStr[8];
  snprintf(minStr, sizeof(minStr), "%.1f", minTemp);
  
  gfx->getTextBounds(minStr, 0, 0, &x1, &y1, &w, &h);
  int minTextX = minTempX - w / 2;
  int minTextY = minTempY + textOffset;

  if (minTextX < FB_X_OFFSET) minTextX = FB_X_OFFSET;
  if (minTextX + w > FB_X_OFFSET + FB_WIDTH) minTextX = FB_X_OFFSET + FB_WIDTH - w;
  if (minTextY + h > FB_Y_OFFSET + FB_HEIGHT) minTextY = minTempY - textOffset - h;
  
  gfx->setCursor(minTextX, minTextY);
  gfx->print(minStr);
}

// ==========================================
// LEGEND (LEFT PANEL)
// ==========================================

void drawLegend(float tMin, float tMax, float fps) {
  if (firstTempUpdate) {
    smoothedMinTemp = tMin;
    smoothedMaxTemp = tMax;
    firstTempUpdate = false;
  } else {
    smoothedMinTemp = smoothedMinTemp * TEMP_DISPLAY_SMOOTH + tMin * (1.0f - TEMP_DISPLAY_SMOOTH);
    smoothedMaxTemp = smoothedMaxTemp * TEMP_DISPLAY_SMOOTH + tMax * (1.0f - TEMP_DISPLAY_SMOOTH);
  }

  if (!legendInitialized) {
    gfx->fillRect(LEGEND_X, LEGEND_Y, LEGEND_WIDTH, LEGEND_HEIGHT, COL_PANEL_BG);
    gfx->drawRect(LEGEND_X, LEGEND_Y, LEGEND_WIDTH, LEGEND_HEIGHT, COL_TEXT);
    legendInitialized = true;
  }

  for (int i = 0; i < LEGEND_SCALE_H; i++) {
    float normalized = 1.0f - (float)i / LEGEND_SCALE_H;
    int lutIdx = (int)(normalized * (COLOR_LUT_SIZE - 1));
    lutIdx = constrain(lutIdx, 0, COLOR_LUT_SIZE - 1);
    uint16_t color = colorLUT[lutIdx];
    gfx->drawFastHLine(LEGEND_X + LEGEND_SCALE_X, LEGEND_Y + LEGEND_SCALE_Y + i, LEGEND_SCALE_W, color);
  }
  
  gfx->drawRect(LEGEND_X + LEGEND_SCALE_X - 1, LEGEND_Y + LEGEND_SCALE_Y - 1, 
  LEGEND_SCALE_W + 2, LEGEND_SCALE_H + 2, COL_TEXT);
  
  gfx->setTextSize(LEGEND_LABEL_SIZE);
  gfx->setTextColor(COL_TEXT, COL_PANEL_BG);
  
  gfx->fillRect(LEGEND_X + 4, LEGEND_Y + 10, LEGEND_WIDTH - 8, 20, COL_PANEL_BG);
  gfx->setCursor(LEGEND_X + 8, LEGEND_Y + 12);
  gfx->println("MAX");
  gfx->setCursor(LEGEND_X + 8, LEGEND_Y + 20);
  gfx->setTextColor(COL_ACCENT, COL_PANEL_BG);
  gfx->print(smoothedMaxTemp, 1);
  gfx->println("C");
  
  gfx->setTextColor(COL_TEXT, COL_PANEL_BG);
  gfx->fillRect(LEGEND_X + 4, LEGEND_Y + LEGEND_SCALE_Y + LEGEND_SCALE_H + 10, LEGEND_WIDTH - 8, 20, COL_PANEL_BG);
  gfx->setCursor(LEGEND_X + 8, LEGEND_Y + LEGEND_SCALE_Y + LEGEND_SCALE_H + 12);
  gfx->println("MIN");
  gfx->setCursor(LEGEND_X + 8, LEGEND_Y + LEGEND_SCALE_Y + LEGEND_SCALE_H + 20);
  gfx->setTextColor(rgb565(100, 200, 255), COL_PANEL_BG);
  gfx->print(smoothedMinTemp, 1);
  gfx->println("C");
  
  gfx->fillRect(LEGEND_X + 4, LEGEND_Y + 280, LEGEND_WIDTH - 8, 30, COL_PANEL_BG);
  gfx->setTextColor(COL_TEXT, COL_PANEL_BG);
  gfx->setCursor(LEGEND_X + 10, LEGEND_Y + 282);
  gfx->println("FPS");
  gfx->setTextColor(COL_RGB_TEXT, COL_PANEL_BG);
  gfx->setCursor(LEGEND_X + 10, LEGEND_Y + 290);
  gfx->print(fps, 1);
}

// ==========================================
// MENU (RIGHT PANEL)
// ==========================================

void drawMenu(DisplayMode currentMode) {
  for (int i = 0; i < MODE_COUNT; i++) {
    menuItemTargetAlpha[i] = (i == currentMode) ? 1.0f : 0.3f;
  }
  
  bool needsUpdate = false;
  for (int i = 0; i < MODE_COUNT; i++) {
    if (abs(menuItemAlpha[i] - menuItemTargetAlpha[i]) > 0.01f) {
      menuItemAlpha[i] += (menuItemTargetAlpha[i] - menuItemAlpha[i]) * MENU_ANIM_SPEED;
      needsUpdate = true;
    } else {
      menuItemAlpha[i] = menuItemTargetAlpha[i];
    }
  }
  
  if (!menuInitialized || needsUpdate) {
    gfx->fillRect(MENU_X, MENU_Y, MENU_WIDTH, MENU_HEIGHT, COL_PANEL_BG);
    gfx->drawRect(MENU_X, MENU_Y, MENU_WIDTH, MENU_HEIGHT, COL_TEXT);
    
    const char* menuLabels[] = {"LIVE", "PAUSE", "RGB", "CHRG"};
    const char* menuIcons[] = {">", "||", "~", "Z"};
    
    for (int i = 0; i < MODE_COUNT; i++) {
      int y = MENU_Y + i * MENU_ITEM_HEIGHT;
      
      uint16_t bgColor = COL_PANEL_BG;
      uint16_t borderColor = blendColor(COL_PANEL_BG, COL_MENU_SELECTED, menuItemAlpha[i]);
      uint16_t textColor = blendColor(rgb565(80, 80, 80), COL_TEXT, menuItemAlpha[i]);
      uint16_t iconColor = blendColor(rgb565(60, 60, 60), COL_LIVE, menuItemAlpha[i]);
      
      gfx->fillRect(MENU_X + 4, y + 4, MENU_WIDTH - 8, MENU_ITEM_HEIGHT - 8, bgColor);
      
      if (i == currentMode) {
        gfx->drawRect(MENU_X + 4, y + 4, MENU_WIDTH - 8, MENU_ITEM_HEIGHT - 8, borderColor);
        gfx->drawRect(MENU_X + 5, y + 5, MENU_WIDTH - 10, MENU_ITEM_HEIGHT - 10, borderColor);
      }

      gfx->setTextSize(MENU_ICON_SIZE);
      gfx->setTextColor(iconColor, bgColor);
      
      int16_t x1, y1;
      uint16_t w, h;
      gfx->getTextBounds(menuIcons[i], 0, 0, &x1, &y1, &w, &h);
      int iconX = MENU_X + (MENU_WIDTH - w) / 2;
      int iconY = y + 15;
      
      gfx->setCursor(iconX, iconY);
      gfx->println(menuIcons[i]);
    
      gfx->setTextSize(MENU_TEXT_SIZE);
      gfx->setTextColor(textColor, bgColor);
      
      gfx->getTextBounds(menuLabels[i], 0, 0, &x1, &y1, &w, &h);
      int textX = MENU_X + (MENU_WIDTH - w) / 2;
      int textY = y + 48;
      
      gfx->setCursor(textX, textY);
      gfx->println(menuLabels[i]);
    }
    
    menuInitialized = true;
  }
}

// ==========================================
// CHARGING SCREEN
// ==========================================

static bool chargingScreenInitialized = false;

void drawChargingScreen() {
  if (!chargingScreenInitialized) {
    gfx->fillRect(FB_X_OFFSET, FB_Y_OFFSET, FB_WIDTH, FB_HEIGHT, COL_BG);
    gfx->setTextSize(TEXT_SIZE_MEDIUM);
    
    int16_t x1, y1;
    uint16_t w, h;
    gfx->getTextBounds("CHARGING", 0, 0, &x1, &y1, &w, &h);
    int textX = FB_X_OFFSET + (FB_WIDTH - w) / 2;
    int textY = FB_Y_OFFSET + (FB_HEIGHT - h) / 2 - 20;

    gfx->setTextColor(COL_TEXT, COL_BG);
    gfx->setCursor(textX, textY);
    gfx->println("CHARGING");
    
    gfx->setTextSize(TEXT_SIZE_SMALL);
    gfx->getTextBounds("MODE", 0, 0, &x1, &y1, &w, &h);
    textX = FB_X_OFFSET + (FB_WIDTH - w) / 2;
    gfx->setCursor(textX, textY + 20);
    gfx->println("MODE");
    
    chargingScreenInitialized = true;
  }
}

void resetDisplayState() {
  firstTempUpdate = true;
  menuInitialized = false;
  legendInitialized = false;
  chargingScreenInitialized = false; 
  smoothedMinTemp = 0.0f;
  smoothedMaxTemp = 0.0f;
  
}
void setDisplayBrightness(uint8_t level) {
  analogWrite(TFT_LED_PIN, level);
}