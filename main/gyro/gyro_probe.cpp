#include "gyro.hpp"

#include "gyro_mpu6050.hpp"
#include "gyro_bmi160.hpp"
#include "gyro_icm42688.hpp"

extern "C" {
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <esp_log.h>
#include "bus/mini_i2c.h"
}

#include "global_context.hpp"

#include <cstdint>
#include <initializer_list>

void gyro_probe_and_start_task(GyroHal* hal) {
    struct probe_entry {
        uint8_t i2c_addr;
        bool (*probe_ptr)(uint8_t);
        void (*task_ptr)(void*);
    };
    static const std::initializer_list<probe_entry> probe_list = {
        {0x68, probe_bmi160, gyro_bmi160_task},     {0x69, probe_bmi160, gyro_bmi160_task},
        {0x68, probe_mpu6050, gyro_mpu6050_task},   {0x69, probe_mpu6050, gyro_mpu6050_task},
        {0x68, probe_icm42688, gyro_icm42688_task}, {0x69, probe_icm42688, gyro_icm42688_task}};

    for (auto& entry : probe_list) {
        mini_i2c_hw_fsm_reset();
        for (int i = 0; i < 3; ++i) {
            ESP_LOGI("gyro-prober", "Probing %02X", entry.i2c_addr);
            if (entry.probe_ptr(entry.i2c_addr)) {
                hal->i2c_adr = entry.i2c_addr;
                xTaskCreate(entry.task_ptr, "gyro-task", 4096, (void*)hal, configMAX_PRIORITIES - 1,
                            nullptr);
                return;
            }
            vTaskDelay(50 / portTICK_PERIOD_MS);
        }
    }
    ESP_LOGE("gyro_probe", "No gyro found!");
}

bool gyro_hal_init(GyroHal* hal, int sda, int scl) {
    if (sda >= 0 && scl >= 0) {
        ESP_ERROR_CHECK(mini_i2c_init(sda, scl, 400000));
        gyro_probe_and_start_task(hal);
    } else {
        ESP_LOGW("gyro", "Please assign i2c gpio pins!");
        return false;
    }
    return true;
}
