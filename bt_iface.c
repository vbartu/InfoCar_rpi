
#include <stdio.h>
#include "gdbus/gdbus.h"

/*
 * This file is used to communicate with the BLE Server (running
 * in another process), using DBUS.
 */

static GDBusProxy *acc_x, *acc_z, *gyro_x, *gyro_z, *temp, *speed, *control;
static GDBusClient *client;
static DBusConnection *connection;
static int con= 0;
static int32_t control_flag = 0;

/*
 * Connects to the DBUS, and configure the proxies, which grants access
 * to read or write each characteristic.
 */
void set_up_bt_iface() {
	connection = g_dbus_setup_bus(DBUS_BUS_SYSTEM, NULL, NULL);
	client = g_dbus_client_new(connection, "com.infocar", "/");

	acc_x = g_dbus_proxy_new(client, "/service1/characteristic1", "org.bluez.GattCharacteristic1");
	acc_z = g_dbus_proxy_new(client, "/service1/characteristic2", "org.bluez.GattCharacteristic1");
	gyro_x = g_dbus_proxy_new(client, "/service1/characteristic3", "org.bluez.GattCharacteristic1");
	gyro_z = g_dbus_proxy_new(client, "/service1/characteristic4", "org.bluez.GattCharacteristic1");
	temp = g_dbus_proxy_new(client, "/service1/characteristic5", "org.bluez.GattCharacteristic1");
	speed = g_dbus_proxy_new(client, "/service1/characteristic6", "org.bluez.GattCharacteristic1");

	control = g_dbus_proxy_new(client, "/service1/characteristic7", "org.bluez.GattCharacteristic1");
}

/*
 * Configures the message to be sent over DBUS to write a value.
 */
void wr_setup(DBusMessageIter *iter, void *user_data) {
	int32_t numero = *((int32_t*) user_data);

	// Decompose the value in bytes
	char c1 = (numero >> 24) & 0xFF;
	char c2 = (numero >> 16) & 0xFF;
	char c3 = (numero >> 8) & 0xFF;
	char c4 = numero & 0xFF;

	DBusMessageIter value, dict;

	// Creates an array of bytes.
	dbus_message_iter_open_container(iter, DBUS_TYPE_ARRAY, "y", &value);
	dbus_message_iter_append_basic(&value, DBUS_TYPE_BYTE, &c1);
	dbus_message_iter_append_basic(&value, DBUS_TYPE_BYTE, &c2);
	dbus_message_iter_append_basic(&value, DBUS_TYPE_BYTE, &c3);
	dbus_message_iter_append_basic(&value, DBUS_TYPE_BYTE, &c4);
	dbus_message_iter_close_container(iter, &value);

	// Empty options
	dbus_message_iter_open_container(iter, DBUS_TYPE_ARRAY, "{sv}", &dict);
	dbus_message_iter_close_container(iter, &dict);

}

/*
 * Checks for errors from a write requests.
 */
void wr_reply(DBusMessage *reply, void *user_data) {

	//printf("cago2en...\n");
	DBusError derr;

	dbus_error_init(&derr);
	dbus_set_error_from_message(&derr, reply);

	if (dbus_error_is_set(&derr))
		printf("Value Changed: %s\n", derr.message);
	else
		//printf("Value Changed: OK\n");

	dbus_error_free(&derr);
}

/*
 * This method receives the array with the collected data, and writes
 * each value in its corresponding characteristic.
 */
void bt_write(int32_t data[]) {
	int32_t *ptr = data;

	g_dbus_proxy_method_call(acc_x, "WriteValue", wr_setup, wr_reply, ptr++, NULL);
	g_dbus_proxy_method_call(acc_z, "WriteValue", wr_setup, wr_reply, ptr++, NULL);
	g_dbus_proxy_method_call(gyro_x, "WriteValue", wr_setup, wr_reply, ptr++, NULL);
	g_dbus_proxy_method_call(gyro_z, "WriteValue", wr_setup, wr_reply, ptr++, NULL);
	g_dbus_proxy_method_call(temp, "WriteValue", wr_setup, wr_reply, ptr++, NULL);
	g_dbus_proxy_method_call(speed, "WriteValue", wr_setup, wr_reply, ptr++, NULL);
}

/*
 * Configures the message to be sent over DBUS to read a value (Empty message).
 */
static void rd_setup(DBusMessageIter *iter, void *user_data)
{
	DBusMessageIter value, dict;

	dbus_message_iter_open_container(iter, DBUS_TYPE_ARRAY, "{sv}", &dict);
	dbus_message_iter_close_container(iter, &dict);

}

/*
 * Reads the response received from a read request.
 */
void rd_reply(DBusMessage *reply, void *user_data) {

	// A reply has arrived
	con = 1;

	DBusMessageIter iter, sub;
	dbus_message_iter_init (reply, &iter);
	dbus_message_iter_recurse(&iter, &sub);

	// Reads each byte of the array
	char c1, c2, c3, c4;
	dbus_message_iter_get_basic(&sub, &c1);
	dbus_message_iter_next(&sub);
	dbus_message_iter_get_basic(&sub, &c2);
	dbus_message_iter_next(&sub);
	dbus_message_iter_get_basic(&sub, &c3);
	dbus_message_iter_next(&sub);
	dbus_message_iter_get_basic(&sub, &c4);

	// Reconstructs the value from the array of bytes
	int32_t numero = 0;
	numero = (c1 << 24) | (c2 << 16) | (c3 << 8) | c4;
	* (int32_t *) user_data = numero;
}

/*
 * This method returns the control value read from the characteristic.
 */
int32_t bt_read_control() {
	g_dbus_proxy_method_call(control, "ReadValue", rd_setup, rd_reply, &control_flag, NULL);

	// Waits until a response is received.
	while (dbus_connection_read_write_dispatch (connection, 1000) && con == 0) {}

	con = 0;
	return control_flag;
}


