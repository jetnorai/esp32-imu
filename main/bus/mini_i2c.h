#pragma once

#include <esp_err.h>

#include <stdint.h>

typedef enum {
    I2C_STATUS_IDLE,
    I2C_STATUS_TIMEOUT,
    I2C_STATUS_ARB_LOST,
    I2C_STATUS_NACK,
    I2C_STATUS_ACTIVE
} i2c_status_t;

esp_err_t mini_i2c_init(int sda_pin, int scl_pin, int freq);

esp_err_t mini_i2c_read_reg_sync(uint8_t dev_adr, uint8_t reg_adr, uint8_t* bytes, uint8_t n_bytes);
esp_err_t mini_i2c_write_reg_sync(uint8_t dev_adr, uint8_t reg_adr, uint8_t byte);
esp_err_t mini_i2c_write_reg2_sync(uint8_t dev_adr, uint8_t reg_adr, uint8_t byte1, uint8_t byte2);
esp_err_t mini_i2c_write_n_sync(uint8_t* data, int len);

esp_err_t mini_i2c_read_reg_callback(uint8_t dev_adr, uint8_t reg_adr, uint8_t n_bytes,
                                     void (*callback)(void* args), void* callback_args);
esp_err_t mini_i2c_read_reg_get_result(uint8_t* bytes, uint8_t n_bytes);

esp_err_t mini_i2c_hw_fsm_reset();
esp_err_t mini_i2c_set_timing(int freq);

i2c_status_t mini_i2c_get_status();

esp_err_t mini_i2c_double_stop_timing();