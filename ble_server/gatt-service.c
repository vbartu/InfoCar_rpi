#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <errno.h>
#include <stdio.h>
#include <stdbool.h>
#include <unistd.h>
#include <string.h>
#include <sys/signalfd.h>

#include <glib.h>
#include <dbus/dbus.h>

#include "../gdbus/gdbus.h"
#include "../gdbus/error.h"
#include "advertising.h"

// DBUS addresses
#define GATT_MGR_IFACE			"org.bluez.GattManager1"
#define GATT_SERVICE_IFACE		"org.bluez.GattService1"
#define GATT_CHR_IFACE			"org.bluez.GattCharacteristic1"
#define GATT_DESCRIPTOR_IFACE	"org.bluez.GattDescriptor1"

// Principal Service UUID
#define ICAR_UUID		"00006000-0000-1000-8000-00805f9b34fb"

// Control Characteristic UUID
#define CONTROL_UUID	"00006001-0000-1000-8000-00805f9b34fb"

// Data Characteristics UUIDs
#define ACX_UUID		"00006010-0000-1000-8000-00805f9b34fb"
#define ACZ_UUID		"00006020-0000-1000-8000-00805f9b34fb"
#define GYX_UUID		"00006030-0000-1000-8000-00805f9b34fb"
#define GYZ_UUID		"00006040-0000-1000-8000-00805f9b34fb"
#define TEMP_UUID		"00006050-0000-1000-8000-00805f9b34fb"
#define VEL_UUID		"00006060-0000-1000-8000-00805f9b34fb"

// permissions assigned to the characteristics
static const char *chr_level_props[] = { "read", "notify", NULL };
static const char *control_chr_level_props[] = { "read", "write", NULL };

static GMainLoop *main_loop;
static GSList *services;
static DBusConnection *connection;
static GDBusProxy *ad_manager;
static GDBusClient *client;

struct characteristic {
	char *service;
	char *uuid;
	char *path;
	uint8_t *value;
	int vlen;
	const char **props;
};

struct descriptor {
	struct characteristic *chr;
	char *uuid;
	char *path;
	uint8_t *value;
	int vlen;
	const char **props;
};

static gboolean desc_get_uuid(const GDBusPropertyTable *property,
					DBusMessageIter *iter, void *user_data)
{
	struct descriptor *desc = user_data;

	dbus_message_iter_append_basic(iter, DBUS_TYPE_STRING, &desc->uuid);

	return TRUE;
}

static gboolean desc_get_characteristic(const GDBusPropertyTable *property,
					DBusMessageIter *iter, void *user_data)
{
	struct descriptor *desc = user_data;

	dbus_message_iter_append_basic(iter, DBUS_TYPE_OBJECT_PATH,
						&desc->chr->path);

	return TRUE;
}

static bool desc_read(struct descriptor *desc, DBusMessageIter *iter)
{
	DBusMessageIter array;

	dbus_message_iter_open_container(iter, DBUS_TYPE_ARRAY,
					DBUS_TYPE_BYTE_AS_STRING, &array);

	if (desc->vlen && desc->value)
		dbus_message_iter_append_fixed_array(&array, DBUS_TYPE_BYTE,
						&desc->value, desc->vlen);

	dbus_message_iter_close_container(iter, &array);

	return true;
}

static gboolean desc_get_value(const GDBusPropertyTable *property,
					DBusMessageIter *iter, void *user_data)
{
	struct descriptor *desc = user_data;

	//printf("Descriptor(%s): Get(\"Value\")\n", desc->uuid);

	return desc_read(desc, iter);
}

static void desc_write(struct descriptor *desc, const uint8_t *value, int len)
{
	g_free(desc->value);
	desc->value = g_memdup(value, len);
	desc->vlen = len;

	g_dbus_emit_property_changed(connection, desc->path,
					GATT_DESCRIPTOR_IFACE, "Value");
}

static int parse_value(DBusMessageIter *iter, const uint8_t **value, int *len)
{
	DBusMessageIter array;

	if (dbus_message_iter_get_arg_type(iter) != DBUS_TYPE_ARRAY)
		return -EINVAL;

	dbus_message_iter_recurse(iter, &array);
	dbus_message_iter_get_fixed_array(&array, value, len);

	return 0;
}

static void desc_set_value(const GDBusPropertyTable *property,
				DBusMessageIter *iter,
				GDBusPendingPropertySet id, void *user_data)
{
	struct descriptor *desc = user_data;
	const uint8_t *value;
	int len;

	//printf("Descriptor(%s): Set(\"Value\", ...)\n", desc->uuid);

	if (parse_value(iter, &value, &len)) {
		//printf("Invalid value for Set('Value'...)\n");
		g_dbus_pending_property_error(id,
					ERROR_INTERFACE ".InvalidArguments",
					"Invalid arguments in method call");
		return;
	}

	desc_write(desc, value, len);

	g_dbus_pending_property_success(id);
}

static gboolean desc_get_props(const GDBusPropertyTable *property,
					DBusMessageIter *iter, void *data)
{
	struct descriptor *desc = data;
	DBusMessageIter array;
	int i;

	dbus_message_iter_open_container(iter, DBUS_TYPE_ARRAY,
					DBUS_TYPE_STRING_AS_STRING, &array);

	for (i = 0; desc->props[i]; i++)
		dbus_message_iter_append_basic(&array,
					DBUS_TYPE_STRING, &desc->props[i]);

	dbus_message_iter_close_container(iter, &array);

	return TRUE;
}

static const GDBusPropertyTable desc_properties[] = {
	{ "UUID",		"s", desc_get_uuid },
	{ "Characteristic",	"o", desc_get_characteristic },
	{ "Value",		"ay", desc_get_value, desc_set_value, NULL },
	{ "Flags",		"as", desc_get_props, NULL, NULL },
	{ }
};

static gboolean chr_get_uuid(const GDBusPropertyTable *property,
					DBusMessageIter *iter, void *user_data)
{
	struct characteristic *chr = user_data;

	dbus_message_iter_append_basic(iter, DBUS_TYPE_STRING, &chr->uuid);

	return TRUE;
}

static gboolean chr_get_service(const GDBusPropertyTable *property,
					DBusMessageIter *iter, void *user_data)
{
	struct characteristic *chr = user_data;

	dbus_message_iter_append_basic(iter, DBUS_TYPE_OBJECT_PATH,
							&chr->service);

	return TRUE;
}

static bool chr_read(struct characteristic *chr, DBusMessageIter *iter)
{
	DBusMessageIter array;

	dbus_message_iter_open_container(iter, DBUS_TYPE_ARRAY,
					DBUS_TYPE_BYTE_AS_STRING, &array);

	dbus_message_iter_append_fixed_array(&array, DBUS_TYPE_BYTE,
						&chr->value, chr->vlen);

	dbus_message_iter_close_container(iter, &array);

	return true;
}

static gboolean chr_get_value(const GDBusPropertyTable *property,
					DBusMessageIter *iter, void *user_data)
{
	struct characteristic *chr = user_data;

	//printf("Characteristic(%s): Get(\"Value\")\n", chr->uuid);

	return chr_read(chr, iter);
}

static gboolean chr_get_props(const GDBusPropertyTable *property,
					DBusMessageIter *iter, void *data)
{
	struct characteristic *chr = data;
	DBusMessageIter array;
	int i;

	dbus_message_iter_open_container(iter, DBUS_TYPE_ARRAY,
					DBUS_TYPE_STRING_AS_STRING, &array);

	for (i = 0; chr->props[i]; i++)
		dbus_message_iter_append_basic(&array,
					DBUS_TYPE_STRING, &chr->props[i]);

	dbus_message_iter_close_container(iter, &array);

	return TRUE;
}

static void chr_write(struct characteristic *chr, const uint8_t *value, int len)
{
	g_free(chr->value);
	chr->value = g_memdup(value, len);
	chr->vlen = len;

	g_dbus_emit_property_changed(connection, chr->path, GATT_CHR_IFACE,
								"Value");
}

static void chr_set_value(const GDBusPropertyTable *property,
				DBusMessageIter *iter,
				GDBusPendingPropertySet id, void *user_data)
{
	struct characteristic *chr = user_data;
	const uint8_t *value;
	int len;

	//printf("Characteristic(%s): Set('Value', ...)\n", chr->uuid);

	if (!parse_value(iter, &value, &len)) {
		//printf("Invalid value for Set('Value'...)\n");
		g_dbus_pending_property_error(id,
					ERROR_INTERFACE ".InvalidArguments",
					"Invalid arguments in method call");
		return;
	}

	chr_write(chr, value, len);

	g_dbus_pending_property_success(id);
}

static const GDBusPropertyTable chr_properties[] = {
	{ "UUID",	"s", chr_get_uuid },
	{ "Service",	"o", chr_get_service },
	{ "Value",	"ay", chr_get_value, chr_set_value, NULL },
	{ "Flags",	"as", chr_get_props, NULL, NULL },
	{ }
};

static gboolean service_get_primary(const GDBusPropertyTable *property,
					DBusMessageIter *iter, void *user_data)
{
	dbus_bool_t primary = TRUE;


	//printf("Get Primary: %s\n", primary ? "True" : "False");


	dbus_message_iter_append_basic(iter, DBUS_TYPE_BOOLEAN, &primary);

	return TRUE;
}

static gboolean service_get_uuid(const GDBusPropertyTable *property,
					DBusMessageIter *iter, void *user_data)
{
	const char *uuid = user_data;


	//printf("Get UUID: %s\n", uuid);


	dbus_message_iter_append_basic(iter, DBUS_TYPE_STRING, &uuid);

	return TRUE;
}

static gboolean service_get_includes(const GDBusPropertyTable *property,
					DBusMessageIter *iter, void *user_data)
{
	const char *uuid = user_data;


	//printf("Get Includes: %s\n", uuid);


	return TRUE;
}

static gboolean service_exist_includes(const GDBusPropertyTable *property,
							void *user_data)
{
	const char *uuid = user_data;



	//printf("Exist Includes: %s\n", uuid);


	return FALSE;
}

static const GDBusPropertyTable service_properties[] = {
	{ "Primary", "b", service_get_primary },
	{ "UUID", "s", service_get_uuid },
	{ "Includes", "ao", service_get_includes, NULL,
					service_exist_includes },
	{ }
};

static void chr_iface_destroy(gpointer user_data)
{
	struct characteristic *chr = user_data;

	g_free(chr->uuid);
	g_free(chr->service);
	g_free(chr->value);
	g_free(chr->path);
	g_free(chr);
}

static void desc_iface_destroy(gpointer user_data)
{
	struct descriptor *desc = user_data;

	g_free(desc->uuid);
	g_free(desc->value);
	g_free(desc->path);
	g_free(desc);
}

static int parse_options(DBusMessageIter *iter, const char **device)
{
	DBusMessageIter dict;

	if (dbus_message_iter_get_arg_type(iter) != DBUS_TYPE_ARRAY)
		return -EINVAL;

	dbus_message_iter_recurse(iter, &dict);

	while (dbus_message_iter_get_arg_type(&dict) == DBUS_TYPE_DICT_ENTRY) {
		const char *key;
		DBusMessageIter value, entry;
		int var;

		dbus_message_iter_recurse(&dict, &entry);
		dbus_message_iter_get_basic(&entry, &key);

		dbus_message_iter_next(&entry);
		dbus_message_iter_recurse(&entry, &value);

		var = dbus_message_iter_get_arg_type(&value);
		if (strcasecmp(key, "device") == 0) {
			if (var != DBUS_TYPE_OBJECT_PATH)
				return -EINVAL;
			dbus_message_iter_get_basic(&value, device);


			//printf("Device: %s\n", *device);

		}

		dbus_message_iter_next(&dict);
	}

	return 0;
}

static DBusMessage *chr_read_value(DBusConnection *conn, DBusMessage *msg,
							void *user_data)
{
	struct characteristic *chr = user_data;
	DBusMessage *reply;
	DBusMessageIter iter;
	const char *device;

	if (!dbus_message_iter_init(msg, &iter))
		return g_dbus_create_error(msg, DBUS_ERROR_INVALID_ARGS,
							"Invalid arguments");

	if (parse_options(&iter, &device))
		return g_dbus_create_error(msg, DBUS_ERROR_INVALID_ARGS,
							"Invalid arguments");

	reply = dbus_message_new_method_return(msg);
	if (!reply)
		return g_dbus_create_error(msg, DBUS_ERROR_NO_MEMORY,
							"No Memory");

	dbus_message_iter_init_append(reply, &iter);

	chr_read(chr, &iter);

	return reply;
}

static DBusMessage *chr_write_value(DBusConnection *conn, DBusMessage *msg,
							void *user_data)
{
	struct characteristic *chr = user_data;
	DBusMessageIter iter;
	const uint8_t *value;
	int len;
	const char *device;

	dbus_message_iter_init(msg, &iter);

	if (parse_value(&iter, &value, &len))
		return g_dbus_create_error(msg, DBUS_ERROR_INVALID_ARGS,
							"Invalid arguments");

	if (parse_options(&iter, &device))
		return g_dbus_create_error(msg, DBUS_ERROR_INVALID_ARGS,
							"Invalid arguments");

	chr_write(chr, value, len);

	return dbus_message_new_method_return(msg);
}

static DBusMessage *chr_start_notify(DBusConnection *conn, DBusMessage *msg,
							void *user_data)
{
	return g_dbus_create_error(msg, DBUS_ERROR_NOT_SUPPORTED,
							"Not Supported");
}

static DBusMessage *chr_stop_notify(DBusConnection *conn, DBusMessage *msg,
							void *user_data)
{
	return g_dbus_create_error(msg, DBUS_ERROR_NOT_SUPPORTED,
							"Not Supported");
}

static const GDBusMethodTable chr_methods[] = {
	{ GDBUS_ASYNC_METHOD("ReadValue", GDBUS_ARGS({ "options", "a{sv}" }),
					GDBUS_ARGS({ "value", "ay" }),
					chr_read_value) },
	{ GDBUS_ASYNC_METHOD("WriteValue", GDBUS_ARGS({ "value", "ay" },
						{ "options", "a{sv}" }),
					NULL, chr_write_value) },
	{ GDBUS_ASYNC_METHOD("StartNotify", NULL, NULL, chr_start_notify) },
	{ GDBUS_METHOD("StopNotify", NULL, NULL, chr_stop_notify) },
	{ }
};

static DBusMessage *desc_read_value(DBusConnection *conn, DBusMessage *msg,
							void *user_data)
{
	struct descriptor *desc = user_data;
	DBusMessage *reply;
	DBusMessageIter iter;
	const char *device;

	if (!dbus_message_iter_init(msg, &iter))
		return g_dbus_create_error(msg, DBUS_ERROR_INVALID_ARGS,
							"Invalid arguments");

	if (parse_options(&iter, &device))
		return g_dbus_create_error(msg, DBUS_ERROR_INVALID_ARGS,
							"Invalid arguments");

	reply = dbus_message_new_method_return(msg);
	if (!reply)
		return g_dbus_create_error(msg, DBUS_ERROR_NO_MEMORY,
							"No Memory");

	dbus_message_iter_init_append(reply, &iter);

	desc_read(desc, &iter);

	return reply;
}

static DBusMessage *desc_write_value(DBusConnection *conn, DBusMessage *msg,
							void *user_data)
{
	struct descriptor *desc = user_data;
	DBusMessageIter iter;
	const char *device;
	const uint8_t *value;
	int len;

	if (!dbus_message_iter_init(msg, &iter))
		return g_dbus_create_error(msg, DBUS_ERROR_INVALID_ARGS,
							"Invalid arguments");

	if (parse_value(&iter, &value, &len))
		return g_dbus_create_error(msg, DBUS_ERROR_INVALID_ARGS,
							"Invalid arguments");

	if (parse_options(&iter, &device))
		return g_dbus_create_error(msg, DBUS_ERROR_INVALID_ARGS,
							"Invalid arguments");

	desc_write(desc, value, len);

	return dbus_message_new_method_return(msg);
}

static const GDBusMethodTable desc_methods[] = {
	{ GDBUS_ASYNC_METHOD("ReadValue", GDBUS_ARGS({ "options", "a{sv}" }),
					GDBUS_ARGS({ "value", "ay" }),
					desc_read_value) },
	{ GDBUS_ASYNC_METHOD("WriteValue", GDBUS_ARGS({ "value", "ay" },
						{ "options", "a{sv}" }),
					NULL, desc_write_value) },
	{ }
};

// Registers the Characteristics
static gboolean register_characteristic(const char *chr_uuid,
						const uint8_t *value, int vlen,
						const char **props,
						const char *desc_uuid,
						const char **desc_props,
						const char *service_path)
{
	struct characteristic *chr;
	struct descriptor *desc;
	static int id = 1;

	chr = g_new0(struct characteristic, 1);
	chr->uuid = g_strdup(chr_uuid);
	chr->value = g_memdup(value, vlen);
	chr->vlen = vlen;
	chr->props = props;
	chr->service = g_strdup(service_path);
	chr->path = g_strdup_printf("%s/characteristic%d", service_path, id++);

	if (!g_dbus_register_interface(connection, chr->path, GATT_CHR_IFACE,
					chr_methods, NULL, chr_properties,
					chr, chr_iface_destroy)) {

		printf("Couldn't register characteristic interface\n");

		chr_iface_destroy(chr);
		return FALSE;
	}


	printf("Registered chr: %s\n", chr->path);


	if (!desc_uuid)
		return TRUE;

	desc = g_new0(struct descriptor, 1);
	desc->uuid = g_strdup(desc_uuid);
	desc->chr = chr;
	desc->props = desc_props;
	desc->path = g_strdup_printf("%s/descriptor%d", chr->path, id++);

	if (!g_dbus_register_interface(connection, desc->path,
					GATT_DESCRIPTOR_IFACE,
					desc_methods, NULL, desc_properties,
					desc, desc_iface_destroy)) {

		printf("Couldn't register descriptor interface\n");

		g_dbus_unregister_interface(connection, chr->path,
							GATT_CHR_IFACE);

		desc_iface_destroy(desc);
		return FALSE;
	}

	return TRUE;
}

// Registers the Service
static char *register_service(const char *uuid)
{
	static int id = 1;
	char *path;

	path = g_strdup_printf("/service%d", id++);
	if (!g_dbus_register_interface(connection, path, GATT_SERVICE_IFACE,
				NULL, NULL, service_properties,
				g_strdup(uuid), g_free)) {

		printf("Couldn't register service interface\n");

		g_free(path);
		return NULL;
	}

	return path;
}


/*
 * Creates the DBUS hierarchy that represents the GATT Server, creating
 * the Service DBUS object, and the seven Characteristic DBUS Objects
 * hanging from it.
 */
static void create_infocar_services()
{
	char *service_path;

	// Initial value of the characteristics
	uint32_t init = 0;

	// Registers the Service
	service_path = register_service(ICAR_UUID);
	if (!service_path)
		return;

	// Registers the Characteristics
	register_characteristic(ACX_UUID, &init, sizeof(init), chr_level_props, NULL, NULL, service_path);
	register_characteristic(ACZ_UUID, &init, sizeof(init), chr_level_props, NULL, NULL, service_path);
	register_characteristic(GYX_UUID, &init, sizeof(init), chr_level_props, NULL, NULL, service_path);
	register_characteristic(GYZ_UUID, &init, sizeof(init), chr_level_props, NULL, NULL, service_path);
	register_characteristic(TEMP_UUID, &init, sizeof(init), chr_level_props, NULL, NULL, service_path);
	register_characteristic(VEL_UUID, &init, sizeof(init), chr_level_props, NULL, NULL, service_path);

	register_characteristic(CONTROL_UUID, &init, sizeof(init), control_chr_level_props, NULL, NULL, service_path);

	services = g_slist_prepend(services, service_path);

	printf("Registered service: %s\n", service_path);

}

static void register_app_reply(DBusMessage *reply, void *user_data)
{
	DBusError derr;

	dbus_error_init(&derr);
	dbus_set_error_from_message(&derr, reply);


	if (dbus_error_is_set(&derr))
		printf("RegisterApplication: %s\n", derr.message);
	else
		printf("RegisterApplication: OK\n");



	dbus_error_free(&derr);
}

static void register_app_setup(DBusMessageIter *iter, void *user_data)
{
	const char *path = "/";
	DBusMessageIter dict;

	dbus_message_iter_append_basic(iter, DBUS_TYPE_OBJECT_PATH, &path);

	dbus_message_iter_open_container(iter, DBUS_TYPE_ARRAY, "{sv}", &dict);
	dbus_message_iter_close_container(iter, &dict);
}

static void register_app(GDBusProxy *proxy)
{
	if (!g_dbus_proxy_method_call(proxy, "RegisterApplication",
					register_app_setup, register_app_reply,
					NULL, NULL)) {


		printf("Unable to call RegisterApplication\n");

		return;
	}
}

static void proxy_added_cb(GDBusProxy *proxy, void *user_data)
{
	const char *iface;

	iface = g_dbus_proxy_get_interface(proxy);
	//printf("%s\n", iface);

	if (g_strcmp0(iface, GATT_MGR_IFACE))
		return;

	register_app(proxy);
}

static gboolean signal_handler(GIOChannel *channel, GIOCondition cond,
							gpointer user_data)
{
	static bool __terminated = false;
	struct signalfd_siginfo si;
	ssize_t result;
	int fd;

	if (cond & (G_IO_NVAL | G_IO_ERR | G_IO_HUP))
		return FALSE;

	fd = g_io_channel_unix_get_fd(channel);

	result = read(fd, &si, sizeof(si));
	if (result != sizeof(si))
		return FALSE;

	switch (si.ssi_signo) {
	case SIGINT:
	case SIGTERM:
		if (!__terminated) {
			//printf("Terminating\n");
			g_main_loop_quit(main_loop);
		}

		__terminated = true;
		break;
	}

	return TRUE;
}

static guint setup_signalfd(void)
{
	GIOChannel *channel;
	guint source;
	sigset_t mask;
	int fd;

	sigemptyset(&mask);
	sigaddset(&mask, SIGINT);
	sigaddset(&mask, SIGTERM);

	if (sigprocmask(SIG_BLOCK, &mask, NULL) < 0) {
		perror("Failed to set signal mask");
		return 0;
	}

	fd = signalfd(-1, &mask, 0);
	if (fd < 0) {
		perror("Failed to create signal descriptor");
		return 0;
	}

	channel = g_io_channel_unix_new(fd);

	g_io_channel_set_close_on_unref(channel, TRUE);
	g_io_channel_set_encoding(channel, NULL, NULL);
	g_io_channel_set_buffered(channel, FALSE);

	source = g_io_add_watch(channel,
				G_IO_IN | G_IO_HUP | G_IO_ERR | G_IO_NVAL,
				signal_handler, NULL);

	g_io_channel_unref(channel);

	return source;
}


/*
 * Starts the Bluetooth Low Energy server, registering the services in the DBUS,
 * and starting an infinite loop (main_loop) to attend to data changes sent by
 * the main process.
 */
int ble_server(){
	// Signal to listen for changes or errors.
	static guint signal;
	signal = setup_signalfd();

	// Starts a new DBUS connection under the name "com.infocar".
	connection = g_dbus_setup_bus(DBUS_BUS_SYSTEM, "com.infocar", NULL);

	// Initialize the main_loop.
	main_loop = g_main_loop_new(NULL, FALSE);

	// DBUS object manager, necessary to create the service and the
	// characteristics objects.
	g_dbus_attach_object_manager(connection);

	printf("gatt-service unique name: %s\n",
				dbus_bus_get_unique_name(connection));

	// Creates the services.
	create_infocar_services();

	// Client to register the DBUS service into BLUEZ, creating the GATT SERVER.
	client = g_dbus_client_new(connection, "org.bluez", "/");
	g_dbus_client_set_proxy_handlers(client, proxy_added_cb, NULL, NULL,NULL);

	// Client to register the DBUS advertisement into BLUEZ.
	ad_manager = g_dbus_proxy_new(client, "/org/bluez/hci0", "org.bluez.LEAdvertisingManager1");
	ad_register(connection, ad_manager, "peripheral");

	// Starts the loop.
	g_main_loop_run(main_loop);

	// When the main_loop finish (the program is ended), cleans the memory.
	ad_unregister(connection, ad_manager);
	g_dbus_proxy_unref(ad_manager);
	g_dbus_client_unref(client);
	g_source_remove(signal);
	g_slist_free_full(services, g_free);
	dbus_connection_unref(connection);

	return 0;
}
