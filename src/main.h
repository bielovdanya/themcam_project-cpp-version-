#pragma once


// ================== sensor ==================
#define I2C_SDA_PIN      35       
#define I2C_SCL_PIN      34       
#define I2C_FREQ_HZ      1000000UL 
#define MLX90640_I2C_ADDR 0x33    

#define MLX_W            32        // width
#define MLX_H            24        // height
#define TILE_W           10       
#define TILE_H           10        
#define VIEW_W           240       
#define VIEW_H           320       
#define PANEL_X          240      
#define PANEL_W          240       


// ================== button ==================
#define BTN_PIN          33        
#define BTN_DEBOUNCE_MS  80        
#define BTN_PULLUP       1         


// ================== display ==================
#define TFT_CS_PIN       9         
#define TFT_DC_PIN       11       
#define TFT_RST_PIN      5         
#define TFT_SCK_PIN      13       
#define TFT_MOSI_PIN     12        
#define TFT_MISO_PIN     18       
#define TFT_LED_PIN      10    
#define TFT_SPI_HZ       40000000UL// SPI freq 40 MHz
#define TFT_WIDTH        480       // width
#define TFT_HEIGHT       320       // height


// ================== colors ==================
#define COL_BG           0x0000    // black 
#define COL_TEXT         0xFFFF    // white 
#define COL_ACCENT       0xF800    // red
#define COL_LIVE         0x07E0    // green
#define COL_PAUSED       0xFBE0    // orange


// ================== param ==================
#define TEMP_MIN_CLAMP   -10.0f    
#define TEMP_MAX_CLAMP    80.0f   
#define AUTO_SCALE        1       