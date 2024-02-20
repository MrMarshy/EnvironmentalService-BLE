#include "ble_environmental.h"


LOG_MODULE_REGISTER(ble_custom);

static struct bt_conn *current_conn;
static struct k_work_delayable ble_work;
static volatile bool ble_ready = false;

static struct sensor_value temperature_level_degC;
static struct sensor_value pressure_level;
static struct sensor_value humidity_level;
static struct sensor_value lux_level;

static ssize_t read_lux_cb(struct bt_conn *conn, const struct bt_gatt_attr *attr, void *buf,
	uint16_t len, uint16_t offset);

static ssize_t read_temperature_cb(struct bt_conn *conn, const struct bt_gatt_attr *attr, void *buf,
	uint16_t len, uint16_t offset);

static ssize_t read_pressure_cb(struct bt_conn *conn, const struct bt_gatt_attr *attr, void *buf,
	uint16_t len, uint16_t offset);

static ssize_t read_humidity_cb(struct bt_conn *conn, const struct bt_gatt_attr *attr, void *buf,
	uint16_t len, uint16_t offset);

static void ess_lux_ccc_cfg_changed(const struct bt_gatt_attr *attr, uint16_t value);
static void ess_temp_ccc_cfg_changed(const struct bt_gatt_attr *attr, uint16_t value);
static void ess_press_ccc_cfg_changed(const struct bt_gatt_attr *attr, uint16_t value);
static void ess_humid_ccc_cfg_changed(const struct bt_gatt_attr *attr, uint16_t value);

static enum {
	BLE_DISCONNECTED,
	BLE_ADV_START,
	BLE_ADVERTISING,
	BLE_CONNECTED,
} ble_state;
    

static const struct bt_data ad[] = {
	BT_DATA_BYTES(BT_DATA_FLAGS, (BT_LE_AD_GENERAL | BT_LE_AD_NO_BREDR)),
	BT_DATA_BYTES(BT_DATA_UUID16_ALL, BT_UUID_16_ENCODE(BT_UUID_ESS_VAL)),
};

/* Make sure this is updated to match BT_GATT_SERVICE_DEFINE below */
enum environmental_severice_characterisation_position{
    TEMPERARURE_ATTR_POS = 2,
    PRESSURE_ATTR_POS = 5,
    HUMIDITY_ATTR_POS = 8,
    LUX_ATTR_POS = 11
};

static int bt_le_publish_notification(struct bt_conn *conn, struct sensor_value value, enum environmental_severice_characterisation_position attr_pos);


BT_GATT_SERVICE_DEFINE(ess_srv,  // POS 0
	BT_GATT_PRIMARY_SERVICE(BT_UUID_ESS), // POS1 
	BT_GATT_CHARACTERISTIC(BT_UUID_TEMPERATURE, BT_GATT_CHRC_READ | BT_GATT_CHRC_NOTIFY, BT_GATT_PERM_READ, read_temperature_cb, NULL, &temperature_level_degC), // POS 2
	BT_GATT_CCC(ess_temp_ccc_cfg_changed, // POS 3
        BT_GATT_PERM_READ | BT_GATT_PERM_WRITE), // POS 4
	BT_GATT_CHARACTERISTIC(BT_UUID_PRESSURE, BT_GATT_CHRC_READ | BT_GATT_CHRC_NOTIFY, BT_GATT_PERM_READ, read_pressure_cb, NULL, &pressure_level), // POS 5
	BT_GATT_CCC(ess_press_ccc_cfg_changed, // POS 6
        BT_GATT_PERM_READ | BT_GATT_PERM_WRITE), // POS 7
	BT_GATT_CHARACTERISTIC(BT_UUID_HUMIDITY, BT_GATT_CHRC_READ | BT_GATT_CHRC_NOTIFY, BT_GATT_PERM_READ, read_humidity_cb, NULL, &humidity_level), // POS 8
	BT_GATT_CCC(ess_humid_ccc_cfg_changed, // POS 9
        BT_GATT_PERM_READ | BT_GATT_PERM_WRITE), // POS 10
    BT_GATT_CHARACTERISTIC(BT_UUID_GATT_ILLUM, BT_GATT_CHRC_READ | BT_GATT_CHRC_NOTIFY, BT_GATT_PERM_READ, read_lux_cb, NULL, &lux_level), // POS 11
	BT_GATT_CCC(ess_lux_ccc_cfg_changed, // POS 12
		BT_GATT_PERM_READ | BT_GATT_PERM_WRITE), // POS 13
);


static int init_bt_le_adv(void);
static void bt_ready(int err);
static uint32_t adv_timeout(void);
static void ble_worker_fn(struct k_work *work);
static void on_connected(struct bt_conn *conn, uint8_t err);
static void on_disconnected(struct bt_conn *conn, uint8_t reason);


static struct bt_conn_cb conn_callbacks = {
	.connected = on_connected,
	.disconnected = on_disconnected,
};

int init_bt_le_environmental_service(){
	LOG_INF("init ble environmental service");
	int ret = 0;

	bt_conn_cb_register(&conn_callbacks);

	ret = bt_enable(bt_ready);
	if(ret){
		LOG_ERR("bt_enable failed (err %d)\n", ret);
		ble_state = BLE_DISCONNECTED;
	}
    else{
        LOG_INF("bt enable success!");
		ble_state = BLE_ADV_START;
    }

	k_work_init_delayable(&ble_work, ble_worker_fn);
	k_work_reschedule(&ble_work, K_MSEC(10));

	return ret;
}

static int init_bt_le_adv(void){
   
    LOG_INF("init bt le adv");
    int ret; 
    ret = bt_le_adv_start(BT_LE_ADV_CONN_NAME, ad, ARRAY_SIZE(ad), NULL, 0);
    if(ret){
        LOG_ERR("bt_le_adv_start failed (err %d)\n", ret);
    }
    else{
        LOG_INF("bt le adv start success!");
    }
   
    return ret;
}

static void bt_ready(int err){
	if(err){
		LOG_ERR("bt enble return %d\n", err);
	}
	else{
		LOG_INF("bt ready!\n");
	}
	ble_ready = true;
}

static void on_connected(struct bt_conn *conn, uint8_t err){
	int rc;

	if(err){
		LOG_ERR("Connection failled (err 0x%02x)", err);
	}
	if(ble_state == BLE_ADVERTISING){
		rc = bt_le_adv_stop();
		if(rc < 0){
			LOG_ERR("BLE Advertising stop failed (err 0x%02x)", rc);
		}
	}
	if(!current_conn){
		current_conn = bt_conn_ref(conn);
	}

	ble_state = BLE_CONNECTED;

	k_work_reschedule(&ble_work, K_NO_WAIT);
}

static void on_disconnected(struct bt_conn *conn, uint8_t reason){
    LOG_INF("BLE Disconnected (reason 0x%02x)\n", reason);
	if(current_conn){
		bt_conn_unref(current_conn);
		current_conn = NULL;
	}
	if(ble_state == BLE_CONNECTED){
		ble_state = BLE_DISCONNECTED;
	}

	k_work_reschedule(&ble_work, K_NO_WAIT);
}

static uint32_t adv_timeout(void){
	uint32_t timeout;

	if (bt_rand(&timeout, sizeof(timeout)) < 0) {
		return 10 * MSEC_PER_SEC;
	}

	timeout %= (10 * MSEC_PER_SEC);

	return timeout + (1 * MSEC_PER_SEC);
}


static void ble_worker_fn(struct k_work *work){
	switch(ble_state){

		case BLE_CONNECTED:
			LOG_INF("BLE_CONNECTED EVENT");
			break;
		case BLE_ADV_START:
			LOG_INF("BLE_ADV_START EVENT");
			init_bt_le_adv();
			ble_state = BLE_ADVERTISING;
			k_work_reschedule(&ble_work, K_MSEC(adv_timeout()));
			break;
		case BLE_ADVERTISING:
			LOG_INF("BLE_ADVERTISING EVENT");
			break;
		case BLE_DISCONNECTED:
			LOG_INF("BLE_DISCONNECTED EVENT");
			ble_state = BLE_ADV_START;
			k_work_reschedule(&ble_work, K_NO_WAIT);
		default:
			;
	}
}

static ssize_t read_lux_cb(struct bt_conn *conn, const struct bt_gatt_attr *attr, void *buf,
	uint16_t len, uint16_t offset){

	int32_t value_as_int = lux_level.val1;
	return bt_gatt_attr_read(conn, attr, buf, len, offset, &value_as_int, sizeof(value_as_int));
}

static ssize_t read_temperature_cb(struct bt_conn *conn, const struct bt_gatt_attr *attr, void *buf,
	uint16_t len, uint16_t offset){

	int32_t value_as_int = temperature_level_degC.val1;
	return bt_gatt_attr_read(conn, attr, buf, len, offset, &value_as_int, sizeof(value_as_int));
}

static ssize_t read_pressure_cb(struct bt_conn *conn, const struct bt_gatt_attr *attr, void *buf,
	uint16_t len, uint16_t offset){
	
	int32_t value_as_int = pressure_level.val1;
	return bt_gatt_attr_read(conn, attr, buf, len, offset, &value_as_int, sizeof(value_as_int));
}

static ssize_t read_humidity_cb(struct bt_conn *conn, const struct bt_gatt_attr *attr, void *buf,
	uint16_t len, uint16_t offset){

	int32_t value_as_int = humidity_level.val1;
	return bt_gatt_attr_read(conn, attr, buf, len, offset, &value_as_int, sizeof(value_as_int));
}

static void ess_lux_ccc_cfg_changed(const struct bt_gatt_attr *attr, uint16_t value){

	ARG_UNUSED(attr);

	bool notify_enabled = (value == BT_GATT_CCC_NOTIFY);

	LOG_INF("BH1750 Lux Notifications %s", notify_enabled ? "enabled": "disabled");
}

static void ess_temp_ccc_cfg_changed(const struct bt_gatt_attr *attr, uint16_t value){

	ARG_UNUSED(attr);

	bool notify_enabled = (value == BT_GATT_CCC_NOTIFY);

	LOG_INF("ESS Temperature Notifications %s", notify_enabled ? "enabled": "disabled");
}

static void ess_press_ccc_cfg_changed(const struct bt_gatt_attr *attr, uint16_t value){

	ARG_UNUSED(attr);

	bool notify_enabled = (value == BT_GATT_CCC_NOTIFY);

	LOG_INF("ESS Pressure Notifications %s", notify_enabled ? "enabled": "disabled");
}

static void ess_humid_ccc_cfg_changed(const struct bt_gatt_attr *attr, uint16_t value){

	ARG_UNUSED(attr);

	bool notify_enabled = (value == BT_GATT_CCC_NOTIFY);

	LOG_INF("ESS Humidity Notifications %s", notify_enabled ? "enabled": "disabled");
}

int bt_le_lux_publish_sensor_data(struct sensor_value lux_value){
    lux_level = lux_value;
	return bt_le_publish_notification(current_conn, lux_value, LUX_ATTR_POS);
}

int bt_le_temperature_publish_sensor_data(struct sensor_value temp_value){
    temperature_level_degC = temp_value;
    return bt_le_publish_notification(current_conn, temp_value, TEMPERARURE_ATTR_POS);
}

int bt_le_humidity_publish_sensor_data(struct sensor_value humidity_value){
    humidity_level = humidity_value;
    return bt_le_publish_notification(current_conn, humidity_value, HUMIDITY_ATTR_POS);
}

int bt_le_pressure_publish_sensor_data(struct sensor_value pressure_value){
    pressure_level = pressure_value;
    return bt_le_publish_notification(current_conn, pressure_value, PRESSURE_ATTR_POS);
}


static int bt_le_publish_notification(struct bt_conn *conn, struct sensor_value value, enum environmental_severice_characterisation_position attr_pos){
	struct bt_gatt_notify_params params = {0};
	const struct bt_gatt_attr *attr = NULL;

	int32_t value_as_int = value.val1;
	attr = &ess_srv.attrs[attr_pos];
	params.attr = attr;
	params.data = (void*)&value_as_int;
	params.len = sizeof(value_as_int);
	params.func = NULL;

	if(!conn){
		LOG_DBG("Notification sent to all connected peers");
		return bt_gatt_notify_cb(NULL, &params);
	}
	else if(bt_gatt_is_subscribed(conn, attr, BT_GATT_CCC_NOTIFY)){
		return bt_gatt_notify_cb(conn, &params);
	}
	else{
		return -EINVAL;
	}
}