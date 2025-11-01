#pragma once

// ==========================================
// SENSOR CONFIG
// ==========================================
#define I2C_SDA_PIN         35
#define I2C_SCL_PIN         34
#define I2C_FREQ_HZ         1600000UL
#define MLX90640_I2C_ADDR   0x33
#define MLX_W               32
#define MLX_H               24
#define SENSOR_FPS          16

// ==========================================
// DISPLAY CONFIGURATION 
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
#define FB_WIDTH            408
#define FB_HEIGHT           320
#define PANEL_X             408
#define PANEL_W             72

// ==========================================
// BUTTON CONFIG
// ==========================================
#define BTN_PIN             33
#define BTN_DEBOUNCE_MS     60
#define BTN_LONG_PRESS_MS   1500

// ==========================================
// COLOR SCHEME
// ==========================================
#define COL_BG              0x0000
#define COL_TEXT            0xFFFF
#define COL_ACCENT          0xF800
#define COL_LIVE            0x07E0
#define COL_PAUSED          0xFBE0
#define COL_RGB_TEXT        0xFFE0

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
#define TARGET_FPS          60

// ==========================================
// MEMORY ALLOCATION
// ==========================================
#define FRAMEBUFFER_ENABLED 1
#define USE_DMA_ALLOCATION  1

// ==========================================
// DISPLAY CONSTANTS
// ==========================================
#define DISPLAY_INIT_DELAY  100
#define STARTUP_DELAY_MS    2000
#define RGB_COLOR_STEPS     7
#define COLOR_STEP_SIZE     0.143f
#define LEGEND_HEIGHT       80
#define LEGEND_WIDTH        30
#define PANEL_LABEL_SIZE    1
#define TEXT_SIZE_LARGE     3
#define TEXT_SIZE_SMALL     1
#define PANEL_Y_MIN_LABEL   60
#define PANEL_Y_MIN_UNIT    90
#define PANEL_Y_MAX_LABEL   130
#define PANEL_Y_MAX_UNIT    160
#define PANEL_Y_LIVE_STATUS 20
#define PANEL_Y_MIN_VAL     75
#define PANEL_Y_MAX_VAL     145
#define PANEL_Y_FPS         200
#define PANEL_Y_LEGEND      230
#define PANEL_X_OFFSET      6
#define PANEL_MIN_VAL_W     60
#define PANEL_MIN_VAL_H     12
#define MIN_TEMP_RANGE      0.5f
#define MIN_RANGE_DEFAULT   0.1f
#define TEMP_PRECISION      1
#define FPS_PRECISION       1

// ==========================================
// SENSOR CONSTANTS
// ==========================================
#define MLX_FRAME_SIZE      834
#define FRAME_SMOOTH_COUNT  16

// ==========================================
// TIMING CONSTANTS
// ==========================================
#define SERIAL_BAUD         115200
#define SETUP_DELAY_MS      500
#define SENSOR_INIT_DELAY   100
#define SENSOR_ERROR_WAIT   1000
#define STATS_INTERVAL_MS   1200
#define PAUSE_UPDATE_MS     100
#define PAUSE_DELAY_MS      50
#define MICRO_TO_MS         1000.0f
#define MS_TO_MICRO         1000
#define MAX_TEMP_RANGE      999.0f
#define MIN_TEMP_INIT       -999.0f

// ==========================================
// UI TEXT RENDERING
// ==========================================
#define UI_TEXT_SETUP_X_OFFSET   20
#define UI_TEXT_SETUP_Y_OFFSET   30
#define UI_TEXT_SETUP_Y_SMALL    40
#define UI_ERROR_X               20
#define UI_ERROR_Y_MAIN          80
#define UI_ERROR_Y_SUB           110
#define UI_ERROR_TEXT_SIZE       2
#define UI_CURSOR_X              10
#define UI_CURSOR_Y              50

// ==========================================
// COLOR INTERPOLATION THRESHOLDS
// ==========================================
#define CMAP_BLUE_MAX       255
#define CMAP_BLUE_MIN       100
#define CMAP_GREEN_MAX      255
#define CMAP_GREEN_MID      200
#define CMAP_RED_MID        128
#define CMAP_RED_HALF       0.5f