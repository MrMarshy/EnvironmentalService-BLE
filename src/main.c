#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/sensor.h>

#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/conn.h>
#include <zephyr/bluetooth/gatt.h>
#include <zephyr/bluetooth/uuid.h>

#include "ble_environmental/ble_environmental.h"

LOG_MODULE_REGISTER(ble_ess);

static const struct device *get_bme280_device(void);
static const struct device *get_bh1750_device(void);

#define APP_EVENT_QUEUE_SIZE	8

enum app_event_type{
	APP_BLE_INIT_EVENT,
	APP_BH1750_INIT_EVENT,
	APP_BME280_INIT_EVENT,
    APP_BH1750_READ_EVENT,
    APP_BME280_READ_EVENT,
	APP_BME280_AND_BH1750_READ_EVENT,
	APP_PUBLISH_SENSOR_DATA_FOR_BLE,
	APP_ERROR_EVENT,
    APP_EVENT_UNKNOWN,
    APP_EVENT_MAX = APP_EVENT_UNKNOWN
};

struct app_event{													
	enum app_event_type type;								
	union{															
		struct sensor_value bh1750_data;
		struct sensor_value bme280_data; 									
		int err;													
	};																
};

/* Define message queue */
K_MSGQ_DEFINE(app_event_msq, sizeof(struct app_event), APP_EVENT_QUEUE_SIZE, 4);

/* Timers */
static void app_expiry_function(struct k_timer *tmr);
K_TIMER_DEFINE(app_timer, app_expiry_function, NULL);

static int app_event_manager_push(struct app_event *p_evt);

static int app_event_manager_get(struct app_event *p_evt);

static struct app_event app_evts;

int main(void){

	app_evts.type = APP_BH1750_INIT_EVENT;
	app_event_manager_push(&app_evts);

	const struct device *dev_bme280 = NULL;
	const struct device *dev_bh1750 = NULL;
	struct sensor_value temp, press, humidity, lux;
	int8_t err = -1; 

	while (1){
		app_event_manager_get(&app_evts);

		switch(app_evts.type){

			case APP_BME280_AND_BH1750_READ_EVENT:
				sensor_sample_fetch(dev_bme280);
				sensor_sample_fetch(dev_bh1750);
				sensor_channel_get(dev_bme280, SENSOR_CHAN_AMBIENT_TEMP, &temp);
				sensor_channel_get(dev_bme280, SENSOR_CHAN_PRESS, &press);
				sensor_channel_get(dev_bme280, SENSOR_CHAN_HUMIDITY, &humidity);
				sensor_channel_get(dev_bh1750, SENSOR_CHAN_LIGHT, &lux);

				/* Update values for Read callbacks and Notifications */
				LOG_INF("temp: %d.%06d; press: %d.%06d; humidity: %d.%06d; lux: %d.%06d\n",
					temp.val1, temp.val2, press.val1, press.val2,
					humidity.val1, humidity.val2, lux.val1, lux.val2);

				app_evts.type = APP_PUBLISH_SENSOR_DATA_FOR_BLE;
				k_timer_start(&app_timer, K_MSEC(100), K_MSEC(100));
				break;

			case APP_PUBLISH_SENSOR_DATA_FOR_BLE:
				bt_le_lux_publish_sensor_data(lux);
				bt_le_temperature_publish_sensor_data(temp);
				bt_le_pressure_publish_sensor_data(press);
				bt_le_humidity_publish_sensor_data(humidity);
				app_evts.type = APP_BME280_AND_BH1750_READ_EVENT;
				k_timer_start(&app_timer, K_MSEC(100), K_SECONDS(5));
				break;

			case APP_BH1750_INIT_EVENT:
				dev_bh1750 = get_bh1750_device();

				if(!dev_bh1750){
					app_evts.type = APP_ERROR_EVENT;
					err = 1;
				}
				else{
					app_evts.type = APP_BME280_INIT_EVENT;
				}
				k_timer_start(&app_timer, K_MSEC(2000), K_MSEC(2000));
				break;

			case APP_BME280_INIT_EVENT:
				dev_bme280 = get_bme280_device();
				if(!dev_bme280){
					app_evts.type = APP_ERROR_EVENT;
					err = 2;
				}
				else{
					app_evts.type = APP_BLE_INIT_EVENT;
				}
				break;

			case APP_BLE_INIT_EVENT:
				/* Initialise theh BLE stack */
				k_timer_stop(&app_timer);
				int rc = init_bt_le_environmental_service();
				if(rc != 0){
					LOG_ERR("rc is %d", rc);
					app_evts.type = APP_ERROR_EVENT;
					err = 3;
				}
				else{
					app_evts.type = APP_BME280_AND_BH1750_READ_EVENT;
				}
				k_timer_start(&app_timer, K_SECONDS(2), K_SECONDS(2));
				break;
			
			case APP_ERROR_EVENT:
				if(err == 1){
					LOG_ERR("Error occured: %s", "Falied to init BH1750");
					app_evts.type = APP_BH1750_INIT_EVENT;
				}
				else if(err == 2){
					LOG_ERR("Error occured: %s", "Falied to init BME280");
					app_evts.type = APP_BME280_INIT_EVENT;
				}
				else if(err == 3){
					LOG_ERR("Unable to initialise BLE stack");
					app_evts.type = APP_BLE_INIT_EVENT;
				}
				else{
					LOG_ERR("Unknown error occured");
					app_evts.type = APP_BH1750_INIT_EVENT;
				}
				break;

			default:
				app_evts.type = APP_ERROR_EVENT;
				err = -1;
				break;

		}
		
	}
	return 0;
}

static const struct device *get_bme280_device(void){
	const struct device *const dev = DEVICE_DT_GET_ANY(bosch_bme280);

	if(!dev){
		LOG_ERR("No BME280 device found");
		return NULL;
	}

	if(!device_is_ready(dev)){
		LOG_ERR("Device \"%s\" is not ready. Check the driver intialization logs for errors", dev->name);
		return NULL;
	}

	LOG_INF("Found \"%s\", getting sensor data", dev->name);
	return dev;
}

static const struct device *get_bh1750_device(void){
	const struct device *const dev = DEVICE_DT_GET_ANY(rohm_bh1750);

	if(!dev){
		LOG_ERR("No BH1750 device found");
		return NULL;
	}

	if(!device_is_ready(dev)){
		LOG_ERR("Device \"%s\" is not ready. Check the driver intialization logs for errors", dev->name);
		return NULL;
	}

	LOG_INF("Found \"%s\", getting sensor data", dev->name);
	return dev;
}

static int app_event_manager_push(struct app_event *p_evt){
	
	return k_msgq_put(&app_event_msq, p_evt, K_NO_WAIT);
}

static int app_event_manager_get(struct app_event *p_evt){
	return k_msgq_get(&app_event_msq, p_evt, K_FOREVER);
}

static void app_expiry_function(struct k_timer *tmr){
	(void)tmr;
	app_event_manager_push(&app_evts);
}