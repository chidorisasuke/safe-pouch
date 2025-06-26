#include "stdio.h"
#include "mpu9250.h"
#include "esp_err.h"

static const char *MPU9250_TAG = "mpu9250";

static void mpu9250_link_bus_interface_functions(mpu9250_handle_t *mpu9250_hdl) {
    DRIVER_MPU9250_LINK_IIC_INIT(mpu9250_hdl, mpu9250_interface_iic_init);
    DRIVER_MPU9250_LINK_IIC_DEINIT(mpu9250_hdl, mpu9250_interface_iic_deinit);
    DRIVER_MPU9250_LINK_IIC_READ(mpu9250_hdl, mpu9250_interface_iic_read);
    DRIVER_MPU9250_LINK_IIC_WRITE(mpu9250_hdl, mpu9250_interface_iic_write);
    DRIVER_MPU9250_LINK_SPI_INIT(mpu9250_hdl, mpu9250_interface_spi_init);
    DRIVER_MPU9250_LINK_SPI_DEINIT(mpu9250_hdl, mpu9250_interface_spi_deinit);
    DRIVER_MPU9250_LINK_SPI_READ(mpu9250_hdl, mpu9250_interface_spi_read);
    DRIVER_MPU9250_LINK_SPI_WRITE(mpu9250_hdl, mpu9250_interface_spi_write);
}

esp_err_t mpu9250_init_i2c(mpu9250_handle_t *mpu9250_hdl, i2c_master_bus_config_t *i2c_master_conf,
                           i2c_device_config_t *i2c_dev_conf) {
    esp_err_t ret;


    ESP_LOGD(MPU9250_TAG, "set i2c conf");
    ret = mpu9250_set_i2c_config(i2c_master_conf, i2c_dev_conf);
    assert(ESP_OK == ret);
    ESP_LOGD(MPU9250_TAG, "mpu iface fn links init");
    DRIVER_MPU9250_LINK_INIT(mpu9250_hdl, mpu9250_handle_t);
    DRIVER_MPU9250_LINK_DELAY_MS(mpu9250_hdl, mpu9250_interface_delay_ms);
    DRIVER_MPU9250_LINK_DEBUG_PRINT(mpu9250_hdl, mpu9250_interface_debug_print);
    DRIVER_MPU9250_LINK_RECEIVE_CALLBACK(mpu9250_hdl, mpu9250_debug_receive_callback);
    ESP_LOGD(MPU9250_TAG, "mpu iface bus fn links");
    mpu9250_link_bus_interface_functions(mpu9250_hdl);

    ESP_LOGD(MPU9250_TAG, "mpu iface set");
    ret = mpu9250_set_interface(mpu9250_hdl, MPU9250_INTERFACE_IIC);
    if (ret != 0) {
        ESP_LOGE(MPU9250_TAG, "set i2c interface failed!");
        return ESP_FAIL;
    }
    ESP_LOGD(MPU9250_TAG, "mpu i2c addr set");
    ret = mpu9250_set_addr_pin(mpu9250_hdl, (*i2c_dev_conf).device_address);
    if (ret != 0) {
        ESP_LOGE(MPU9250_TAG, "set i2c address failed!");
        return ESP_FAIL;
    }
    ESP_LOGD(MPU9250_TAG, "mpu init");
    ret = mpu9250_init(mpu9250_hdl);
    if (ret != 0) {
        ESP_LOGE(MPU9250_TAG, "mpu init failed!");
        return ESP_FAIL;
    }
    ESP_LOGD(MPU9250_TAG, "sleeping 100 ms");
    mpu9250_interface_delay_ms(100);
    ret = mpu9250_set_sleep(mpu9250_hdl, MPU9250_BOOL_FALSE);
    if (ret != 0) {
        ESP_LOGE(MPU9250_TAG, "set sleep to false failed");
        (void) mpu9250_deinit(mpu9250_hdl);
        return ESP_FAIL;
    }
    return ret;
}