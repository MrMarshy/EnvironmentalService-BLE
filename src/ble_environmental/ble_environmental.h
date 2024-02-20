#ifndef __BLE_ENVIRONMENTAL_H
#define __BLE_ENVIRONMENTAL_H

#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/conn.h>
#include <zephyr/bluetooth/gatt.h>
#include <zephyr/bluetooth/uuid.h>
#include <zephyr/logging/log.h>
#include <zephyr/drivers/sensor.h>

int init_bt_le_environmental_service();

int bt_le_lux_publish_sensor_data(struct sensor_value lux_value);
int bt_le_temperature_publish_sensor_data(struct sensor_value temp_value);
int bt_le_humidity_publish_sensor_data(struct sensor_value humid_value);
int bt_le_pressure_publish_sensor_data(struct sensor_value lux_value);

#endif // __BLE_ENVIRONMENTAL_H