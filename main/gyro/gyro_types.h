#pragma once

#include <stdint.h>
#include <stdbool.h>

#define GYRO_MAX_QUEUE_LENGTH 1024

typedef struct
{
    uint64_t timestamp;
    int16_t gyro_x, gyro_y, gyro_z;
    int16_t accel_x, accel_y, accel_z;
    uint16_t fifo_backlog;
    uint32_t smpl_interval_ns;
} gyro_sample_message;

#define kMessageGyroScale (2000.0 / 32767.0 * 3.141592 / 180.0)
