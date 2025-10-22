#pragma once

// ==========================================
// SENSOR CONFIG
// ==========================================
#define I2C_SDA_PIN         35
#define I2C_SCL_PIN         34
#define I2C_FREQ_HZ         1000000UL
#define MLX90640_I2C_ADDR   0x33
#define MLX_W               32        // sensor width
#define MLX_H               24        // sensor height
#define SENSOR_FPS          16        // refresh rate in Hz

// ==========================================
// DISPLAY CONFIGURATION (85/15 layout)
// ==========================================
#define TFT_WIDTH           480
#define TFT_HEIGHT          320
#define TFT_CS_PIN          9
#define TFT_DC_PIN          11
#define TFT_RST_PIN         5
#define TFT_SCK_PIN         13
#define TFT_MOSI_PIN        12
#define TFT_MISO_PIN        18
#define TFT_LED_PIN         10
#define TFT_SPI_HZ          80000000UL

// ==========================================
// FRAMEBUFFER DIMENSIONS
// ==========================================
#define FB_WIDTH            408       // 85% of 480
#define FB_HEIGHT           320      
#define PANEL_X             408       
#define PANEL_W             72        // 15% of 480

// ==========================================
// BUTTON CONFIG
// ==========================================
#define BTN_PIN             33
#define BTN_DEBOUNCE_MS     60
#define BTN_LONG_PRESS_MS   1500

// ==========================================
// COLOR SCHEME
// ==========================================
#define COL_BG              0x0000    // Black
#define COL_TEXT            0xFFFF    // White
#define COL_ACCENT          0xF800    // Red
#define COL_LIVE            0x07E0    // Green
#define COL_PAUSED          0xFBE0    // Orange

// ==========================================
// THERMAL PARAMETERS
// ==========================================
#define TEMP_MIN_CLAMP      -10.0f
#define TEMP_MAX_CLAMP      80.0f
#define AUTO_SCALE          1

// ==========================================
// SMOOTHING / PERFORMANCE
// ==========================================
#define FRAME_SMOOTHING     0.65f     
#define TEMP_DISPLAY_SMOOTH 0.80f     
#define TARGET_FPS          60        // target rendering FPS

// ==========================================
// MEMORY ALLOCATION
// ==========================================

#define FRAMEBUFFER_ENABLED 1
#define USE_DMA_ALLOCATION  1