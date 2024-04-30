#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/smf.h>
#include <zephyr/logging/log.h>

#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/gap.h>
#include <zephyr/bluetooth/gatt.h>
#include <zephyr/bluetooth/uuid.h>
#include <zephyr/bluetooth/addr.h>
#include <zephyr/bluetooth/conn.h>

LOG_MODULE_REGISTER(ble_test, LOG_LEVEL_DBG);

/* HTS221 sensor functions and definitions */
/* Replace with the appropriate HTS221 sensor library functions */

// Function to read temperature and humidity from HTS221
void read_sensor_data(float* temperature, float* humidity) {
    // Replace with HTS221 sensor reading implementation
}

// Function to send sensor data to the cloud
void send_data_to_cloud(float temperature, float humidity) {
    // Replace with cloud communication logic
}

/** BEGIN BLE ADVERTISING / SCAN DATA **/

#define BT_UUID_SST_SVC_BYTES  BT_UUID_128_ENCODE(0x00001523, 0x1212, 0xefde, 0x1523, 0x785fdeadbeef)
#define BT_UUID_SST_SVC  BT_UUID_DECLARE_128(BT_UUID_SST_SVC_BYTES)

static struct bt_le_adv_param* adv_param = BT_LE_ADV_PARAM(
    (BT_LE_ADV_OPT_CONNECTABLE|BT_LE_ADV_OPT_USE_IDENTITY), /* Connectable advertising and use identity address */
    800, /*Min Advertising Interval 500ms (800*0.625ms) */
    801, /*Max Advertising Interval 500.625ms (801*0.625ms)*/
    NULL); /* Set to NULL for undirected advertising*/

static const struct bt_data adv_data[] = {
    /* STEP 3.1 - Set the flags and populate the device name in the advertising packet */
    BT_DATA_BYTES(BT_DATA_FLAGS, (BT_LE_AD_GENERAL | BT_LE_AD_NO_BREDR)),
    BT_DATA(BT_DATA_NAME_COMPLETE, CONFIG_BT_DEVICE_NAME, (sizeof(CONFIG_BT_DEVICE_NAME) - 1)),
};

static const struct bt_data scan_data[] = {
    /* STEP 3.2.2 - Include the 16-bytes (128-Bits) UUID of our service in the scan response packet */
    BT_DATA_BYTES(BT_DATA_UUID128_ALL, BT_UUID_SST_SVC_BYTES),
};

/** END BLE ADVERTISING / SCAN DATA **/


/** BEGIN DEFINING BLE GATT SST (SENSOR STREAMING) SERVICE **/

#define BT_UUID_SST_MYSENSOR  BT_UUID_DECLARE_128(BT_UUID_128_ENCODE(0x00001526, 0x1212, 0xefde, 0x1523, 0x785fdeadbeef))

static bool notify_mysensor_enabled;
/// @brief Define the configuration change callback function for the MYSENSOR characteristic
static void sst_ccc_mysensor_cfg_changed(const struct bt_gatt_attr *attr, uint16_t value)
{
    notify_mysensor_enabled = (value == BT_GATT_CCC_NOTIFY);
}

BT_GATT_SERVICE_DEFINE(
    sst_service,
    BT_GATT_PRIMARY_SERVICE(BT_UUID_SST_SVC),
    BT_GATT_CHARACTERISTIC(
        BT_UUID_SST_MYSENSOR,
        BT_GATT_CHRC_NOTIFY,
        BT_GATT_PERM_NONE, 
        NULL, 
        NULL,
        NULL),
    BT_GATT_CCC(
        sst_ccc_mysensor_cfg_changed,
        BT_GATT_PERM_READ | BT_GATT_PERM_WRITE),
);

int my_sst_svc_gatt_notify(float temperature, float humidity)
{
    int err;
    if (!notify_mysensor_enabled) {
        return -EACCES;
    }
    struct {
        float temperature;
        float humidity;
    } sensor_data = {
        .temperature = temperature,
        .humidity = humidity
    };
    err = bt_gatt_notify(NULL, &sst_service.attrs[2], &sensor_data, sizeof(sensor_data));
    if (err < 0) {
        LOG_ERR("Warning: bt_gatt_notify failed with error: %d", err);
    }
    else {
        LOG_DBG("Sent sensor data: temperature=%f, humidity=%f", temperature, humidity);
    }
    return err;
}

/** END DEFINING BLE GATT SST (SENSOR STREAMING) SERVICE **/


/** BEGIN DEFINING BLE CONNECTION MANAGER */

struct bt_conn* our_conn = NULL;

void on_connected(struct bt_conn *conn, uint8_t err) {
    if (err) {
        LOG_ERR("Connection error %d", err);
        return;
    }
    LOG_INF("Connected");
    our_conn = bt_conn_ref(conn);
}

void on_disconnected(struct bt_conn *conn, uint8_t reason)
{
    LOG_INF("Disconnected. Reason %d", reason);
    bt_conn_unref(our_conn);
}

struct bt_conn_cb bt_conn_callbacks = {
    .connected = on_connected,
    .disconnected = on_disconnected,
};

/** END DEFINING BLE CONNECTION MANAGER **/


void main(void) {
    int err;
    float temperature, humidity;
    LOG_INF("Setting up our bluetooth...");

    bt_addr_le_t addr;
    err = bt_addr_le_from_str("FF:EE:DE:AD:BE:EF", "random", &addr);
    if (err) {
        LOG_ERR("Invalid BT address (err %d)\n", err);
        return;
    }

    err = bt_id_create(&addr, NULL);
    if (err < 0) {
        LOG_ERR("Creating new identity failed (err %d)\n", err);
        return;
    }

    bt_conn_cb_register(&bt_conn_callbacks);

    err = bt_enable(NULL);
    if (err < 0) {
        LOG_ERR("Bluetooth init failed (err %d)\n", err);
        return;
    }

    LOG_INF("Bluetooth enabled!");
    LOG_INF("Starting advertising...");

    err = bt_le_adv_start(
        adv_param, adv_data, ARRAY_SIZE(adv_data),
        scan_data, ARRAY_SIZE(scan_data));
    if (err < 0) {
        LOG_ERR("Bluetooth advertisement start failed (err %d)\n", err);
        return;
    }

    LOG_INF("Advertising started!");

    // Predefined values for temperature and humidity
    float predefined_temperature[] = {25.5, 26.0, 26.2, 25.8, 26.5, 25.7, 25.9, 26.3, 26.1, 25.6};
    float predefined_humidity[] = {45.8, 46.2, 47.1, 45.5, 46.8, 45.9, 46.4, 46.6, 47.3, 45.7};

    int num_values = sizeof(predefined_temperature) / sizeof(float);
    int i = 0;

    while (true) {
        // Read sensor data
        temperature = predefined_temperature[i];
        humidity = predefined_humidity[i];

        // Send data to the cloud
        send_data_to_cloud(temperature, humidity);

        // Notify GATT clients with sensor data
        my_sst_svc_gatt_notify(temperature, humidity);

        i++;
        if (i >= num_values) {
            i = 0;
        }

        k_sleep(K_SECONDS(1));
    }
}
