#include "sensor.h"

Adafruit_MLX90640 mlx;

bool initSensor() {
  Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN, I2C_FREQ_HZ); 
  if (!mlx.begin(MLX90640_I2C_ADDR, &Wire)) {       
    return false;                                   
  }
  mlx.setMode(MLX90640_CHESS);                      
  mlx.setResolution(MLX90640_ADC_18BIT);           
  mlx.setRefreshRate(MLX90640_16_HZ);                
  return true;                                     
}

bool readFrame(float *buf) {
  return mlx.getFrame(buf) == 0; \
}
