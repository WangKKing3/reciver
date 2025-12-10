//BLE.c - Receiver SIMPLIFIED VERSION

#include "BLE.h"
#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>
#include <zephyr/bluetooth/gatt.h>
#include <zephyr/bluetooth/uuid.h>

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
static joystick_data_callback_t user_callback = NULL;

/* Scan work for reconnect */
static void restart_scan_work(struct k_work *work);
K_WORK_DELAYABLE_DEFINE(scan_work, restart_scan_work);

static void restart_scan_work(struct k_work *work)
{
	if (!default_conn) {
		printk("Restarting scan...\n");
		ble_start_scan();
	}
}

/* Notification callback */
static uint8_t notify_func(struct bt_conn *conn,
                           struct bt_gatt_subscribe_params *param,
                           const void *buf, uint16_t len)
{
	if (!buf || !len) {
		printk("Unsubscribed\n");
		return BT_GATT_ITER_STOP;
	}

	if (len == sizeof(struct joystick_data) && user_callback) {
		user_callback((const struct joystick_data *)buf);
	}
	return BT_GATT_ITER_CONTINUE;
}

/* GATT discovery */
static uint8_t discover_func(struct bt_conn *conn,
                             const struct bt_gatt_attr *attr,
                             struct bt_gatt_discover_params *param)
{
	int err;

	if (!attr) {
		printk("Discovery complete\n");
		return BT_GATT_ITER_STOP;
	}

	if (param->uuid == &joystick_svc_uuid.uuid) {
		printk("Service found\n");
		discov_param.uuid = &joystick_chr_uuid.uuid;
		discov_param.start_handle = attr->handle + 1;
		discov_param.type = BT_GATT_DISCOVER_CHARACTERISTIC;
		bt_gatt_discover(conn, &discov_param);
	} else if (param->uuid == &joystick_chr_uuid.uuid) {
		printk("Characteristic found\n");
		discov_param.uuid = gatt_ccc_uuid;
		discov_param.start_handle = attr->handle + 2;
		discov_param.type = BT_GATT_DISCOVER_DESCRIPTOR;
		subscribe_param.value_handle = attr->handle + 1;
		bt_gatt_discover(conn, &discov_param);
	} else {
		printk("Subscribing...\n");
		subscribe_param.notify = notify_func;
		subscribe_param.value = BT_GATT_CCC_NOTIFY;
		subscribe_param.ccc_handle = attr->handle;

		err = bt_gatt_subscribe(conn, &subscribe_param);
		if (err && err != -EALREADY) {
			printk("Subscribe failed: %d\n", err);
		} else {
			printk("Subscribed - READY!\n");
		}
	}
	return BT_GATT_ITER_STOP;
}

/* Scan callback */
static void device_found(const bt_addr_le_t *addr, int8_t rssi, uint8_t type,
                         struct net_buf_simple *ad_buf)
{
	if (default_conn || type != BT_GAP_ADV_TYPE_ADV_IND) return;

	while (ad_buf->len > 1) {
		uint8_t len = net_buf_simple_pull_u8(ad_buf);
		if (len == 0U || len > ad_buf->len) return;

		uint8_t ad_type = net_buf_simple_pull_u8(ad_buf);

		if (ad_type == BT_DATA_UUID128_ALL && len == 17) {
			if (!memcmp(ad_buf->data, joystick_svc_uuid.val, 16)) {
				char addr_str[BT_ADDR_LE_STR_LEN];
				bt_addr_le_to_str(addr, addr_str, sizeof(addr_str));
				printk("Found joystick: %s (RSSI %d)\n", addr_str, rssi);

				bt_le_scan_stop();

				/* Connect med default parametere - sender vil oppdatere */
				int err = bt_conn_le_create(addr, BT_CONN_LE_CREATE_CONN,
				                            BT_LE_CONN_PARAM_DEFAULT, &default_conn);
				if (err) {
					printk("Connect failed: %d\n", err);
					k_work_schedule(&scan_work, K_MSEC(500));
				}
				return;
			}
		}
		net_buf_simple_pull(ad_buf, len - 1);
	}
}

/* Connection callbacks */
static void connected(struct bt_conn *conn, uint8_t err)
{
	char addr[BT_ADDR_LE_STR_LEN];
	bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));

	if (err) {
		printk("Connect to %s failed: 0x%02x\n", addr, err);
		if (default_conn) {
			bt_conn_unref(default_conn);
			default_conn = NULL;
		}
		k_work_schedule(&scan_work, K_MSEC(200));
		return;
	}

	printk("Connected: %s\n", addr);

	/* Start discovery */
	discov_param.uuid = &joystick_svc_uuid.uuid;
	discov_param.func = discover_func;
	discov_param.start_handle = BT_ATT_FIRST_ATTRIBUTE_HANDLE;
	discov_param.end_handle = BT_ATT_LAST_ATTRIBUTE_HANDLE;
	discov_param.type = BT_GATT_DISCOVER_PRIMARY;

	err = bt_gatt_discover(default_conn, &discov_param);
	if (err) {
		printk("Discovery failed: %d\n", err);
	}
}

static void disconnected(struct bt_conn *conn, uint8_t reason)
{
	printk("Disconnected: 0x%02x\n", reason);

	if (default_conn) {
		bt_conn_unref(default_conn);
		default_conn = NULL;
	}

	/* Rask reconnect - 100ms */
	k_work_schedule(&scan_work, K_MSEC(100));
}

/* Connection parameter update callback */
static void le_param_updated(struct bt_conn *conn, uint16_t interval,
                             uint16_t latency, uint16_t timeout)
{
	printk("Conn params: interval=%d (%.1fms), timeout=%d (%dms)\n",
	       interval, interval * 1.25, timeout, timeout * 10);
}

static struct bt_conn_cb conn_callbacks = {
	.connected = connected,
	.disconnected = disconnected,
	.le_param_updated = le_param_updated,
};

/* Public API */
int ble_init(joystick_data_callback_t cb)
{
	user_callback = cb;

	int err = bt_enable(NULL);
	if (err) {
		printk("BT init failed: %d\n", err);
		return err;
	}
	printk("Bluetooth ready\n");
	bt_conn_cb_register(&conn_callbacks);
	return 0;
}

int ble_start_scan(void)
{
	if (default_conn) return 0;

	int err = bt_le_scan_start(BT_LE_SCAN_PASSIVE, device_found);
	if (err && err != -EALREADY) {
		printk("Scan failed: %d\n", err);
		return err;
	}
	printk("Scanning for BitPlayer_Joy...\n");
	return 0;
}

bool ble_is_connected(void)
{
	return default_conn != NULL;
}