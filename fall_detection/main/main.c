#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/i2c.h"
#include "mpu9250.h"  // pastikan file header ini tersedia

void app_main(void)
{
    printf("Inisialisasi MPU9250 untuk deteksi jatuh...\n");

    if (mpu9250_init() != ESP_OK) {
        printf("Gagal inisialisasi MPU9250\n");
        return;
    }

    while (1) {
        float ax, ay, az;
        mpu9250_get_acceleration(&ax, &ay, &az);

        float magnitude = sqrt(ax * ax + ay * ay + az * az);
        printf("Accelerometer: ax=%.2f, ay=%.2f, az=%.2f, |a|=%.2f\n", ax, ay, az, magnitude);

        if (magnitude < 0.5 || magnitude > 3.0) {
            printf("⚠️ Deteksi kemungkinan jatuh!\n");
        }

        vTaskDelay(pdMS_TO_TICKS(100));  // Delay 100 ms (~10 Hz sampling)
    }
}
