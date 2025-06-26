#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include "interface/driver_mpu9250_interface.h"
#include "driver_mpu9250.h"
#include "driver/i2c_master.h"


esp_err_t mpu9250_init_i2c(mpu9250_handle_t *mpu9250_hdl, i2c_master_bus_config_t *i2c_master_conf,
                           i2c_device_config_t *i2c_dev_conf);

#ifdef __cplusplus
}
#endif