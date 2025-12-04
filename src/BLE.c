#include "BLE.h"
#include "motor_controls.h"
#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>
#include <zephyr/bluetooth/gatt.h>
#include <zephyr/bluetooth/uuid.h>

//Universally Unique Identifiers
#define JOYSTICK_SVC_UUID \
	BT_UUID_128_ENCODE(0x12345678, 0x1234, 0x5678, 0x1234, 0x56789abcdef0)

#define JOYSTICK_CHR_UUID \
	BT_UUID_128_ENCODE(0x12345678, 0x1234, 0x5678, 0x1234, 0x56789abcdef1)

static struct bt_uuid_128 joystick_svc_uuid = BT_UUID_INIT_128(JOYSTICK_SVC_UUID);
static struct bt_uuid_128 joystick_chr_uuid = BT_UUID_INIT_128(JOYSTICK_CHR_UUID);
static struct bt_uuid *gatt_ccc_uuid = BT_UUID_GATT_CCC;

static struct bt_gatt_discover_params discov_param;
static struct bt_gatt_subscribe_params subscribe_param;
static struct bt_conn *default_conn;


#define DATA_TIMEOUT_MS 200

static struct k_work_delayable timeout_work;
static bool timeout_initialized = false;

// Kalles når ingen data mottas innen timeout
static void data_timeout_handler(struct k_work *work)
{
	printk("Timeout: No data received - starting idle\n");
	motor_start_idle();
}

//kalles hver gang data mottas 
static void reset_timeout(void)
{
	if (timeout_initialized) {
		k_work_reschedule(&timeout_work, K_MSEC(DATA_TIMEOUT_MS));
	}
}

static uint8_t notify_func(struct bt_conn *conn,
                           struct bt_gatt_subscribe_params *param,
                           const void *buf, uint16_t len)
{
	const struct joystick_data *data = buf;

	if (!data || !len) {
		printk("Unsubscribed\n");
		return BT_GATT_ITER_STOP;
	}

	if (len == sizeof(struct joystick_data)) {
		printk("Joystick: X=%d, Y=%d ", data->x_pos, data->y_pos);
		
		motor_stop_idle();
		
		motor_drive_from_joystick(data->x_pos, data->y_pos);
		
		// Reset timeout - vi mottok data
		reset_timeout();
		
	} else {
		printk("Invalid data length: %d\n", len);
	}

	return BT_GATT_ITER_CONTINUE;
}

static uint8_t discover_func(struct bt_conn *conn,
                             const struct bt_gatt_attr *attr,
                             struct bt_gatt_discover_params *param)
{
	int err;

	if (!attr) {
		printk("Discovery complete\n");
		return BT_GATT_ITER_STOP;
	}

	printk("Attribute handle %u\n", attr->handle);

	if (param->uuid == &joystick_svc_uuid.uuid) {
		printk("Joystick service found\n");
		discov_param.uuid = &joystick_chr_uuid.uuid;
		discov_param.start_handle = attr->handle + 1;
		discov_param.type = BT_GATT_DISCOVER_CHARACTERISTIC;

		err = bt_gatt_discover(conn, &discov_param);
		if (err) {
			printk("Characteristic discovery failed (err %d)\n", err);
		}
	} else if (param->uuid == &joystick_chr_uuid.uuid) {
		printk("Joystick characteristic found\n");
		discov_param.uuid = gatt_ccc_uuid;
		discov_param.start_handle = attr->handle + 2;
		discov_param.type = BT_GATT_DISCOVER_DESCRIPTOR;
		subscribe_param.value_handle = attr->handle + 1;

		err = bt_gatt_discover(conn, &discov_param);
		if (err) {
			printk("CCC discovery failed (err %d)\n", err);
		}
	} else {
		printk("CCC found\n");

		subscribe_param.notify = notify_func;
		subscribe_param.value = BT_GATT_CCC_NOTIFY;
		subscribe_param.ccc_handle = attr->handle;

		printk("Subscribing to notifications...\n");
		err = bt_gatt_subscribe(conn, &subscribe_param);
		if (err && err != -EALREADY) {
			printk("Subscribe failed (err %d)\n", err);
		} else {
			printk("Subscribed! Waiting for data...\n");
		}
	}
	// Continue discovery
	return BT_GATT_ITER_STOP;
}

// Device found during scan 
static void device_found(const bt_addr_le_t *addr, int8_t rssi, uint8_t type,
                         struct net_buf_simple *ad_buf)
{
	char addr_str[BT_ADDR_LE_STR_LEN];
	int err;

	if (default_conn) {
		return;
	}

	// Only process advertising data 
	/** Non-connectable and non-scannable advertising. */
	if (type != BT_GAP_ADV_TYPE_ADV_IND) {
		return;
	}

	bt_addr_le_to_str(addr, addr_str, sizeof(addr_str));

	// Parse advertising data 
	while (ad_buf->len > 1) {
		uint8_t len = net_buf_simple_pull_u8(ad_buf);
		uint8_t ad_type;

		if (len == 0U) {
			return;
		}

		if (len > ad_buf->len) {
			return;
		}

		ad_type = net_buf_simple_pull_u8(ad_buf);

		/* Look for our service UUID */
		if (ad_type == BT_DATA_UUID128_ALL && len == 17) {
			if (!memcmp(ad_buf->data, joystick_svc_uuid.val, 16)) {
				printk("Found joystick device: %s\n", addr_str);
				printk("Connecting...\n");
				
				bt_le_scan_stop();
				
				err = bt_conn_le_create(addr, BT_CONN_LE_CREATE_CONN,
				                        BT_LE_CONN_PARAM_DEFAULT,
				                        &default_conn);
				if (err) {
					printk("Connection failed (err %d)\n", err);
				}
				return;
			}
		}

		net_buf_simple_pull(ad_buf, len - 1);
	}
}

// Connection callbacks 
static void connected(struct bt_conn *conn, uint8_t err)
{
	char addr[BT_ADDR_LE_STR_LEN];

	bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));

	if (err) {
		printk("Failed to connect to %s (err %d)\n", addr, err);
		return;
	}

	printk("Connected: %s\n", addr);

	// start service discovery
	discov_param.uuid = &joystick_svc_uuid.uuid;
	discov_param.func = discover_func;
	discov_param.start_handle = BT_ATT_FIRST_ATTRIBUTE_HANDLE;
	discov_param.end_handle = BT_ATT_LAST_ATTRIBUTE_HANDLE;
	discov_param.type = BT_GATT_DISCOVER_PRIMARY;

	err = bt_gatt_discover(default_conn, &discov_param);
	if (err) {
		printk("Discovery failed (err %d)\n", err);
	}
}

static void disconnected(struct bt_conn *conn, uint8_t reason)
{
	char addr[BT_ADDR_LE_STR_LEN];

	bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));

	printk("Disconnected: %s (reason 0x%02x)\n", addr, reason);

	if (default_conn) {
		bt_conn_unref(default_conn);
		default_conn = NULL;
	}

	// Start idle ved disconnect
	motor_start_idle();

	// Restart scanning
	printk("Restarting scan...\n");
	ble_start_scan();
}

static struct bt_conn_cb conn_callbacks = {
	.connected = connected,
	.disconnected = disconnected,
};

// Public functions 
int ble_init(void)
{
	int err;

	printk("\n\n===========================================\n");
	printk("Joystick Receiver\n");
	printk("micro:bit v2 - Zephyr SDK v2.5.1\n");
	printk("===========================================\n\n");

	// Initialize timeout work
	k_work_init_delayable(&timeout_work, data_timeout_handler);
	timeout_initialized = true;
	printk("Timeout system initialized (%d ms)\n", DATA_TIMEOUT_MS);

	// Initialize Bluetooth 
	err = bt_enable(NULL);
	if (err) {
		printk("Bluetooth init failed (err %d)\n", err);
		return err;
	}

	printk("Bluetooth initialized\n");

	bt_conn_cb_register(&conn_callbacks);

	motor_start_idle();

	return 0;
}

int ble_start_scan(void)
{
	int err;

	err = bt_le_scan_start(BT_LE_SCAN_PASSIVE, device_found);
	if (err) {
		printk("Scanning failed to start (err %d)\n", err);
		return err;
	}

	printk("Scanning for devices...\n");
	printk("Looking for: BitPlayer_Joy\n\n");

	return 0;
}