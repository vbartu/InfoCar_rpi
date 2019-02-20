#ifndef GATT_SERVICE_H_
#define GATT_SERVICE_H_

/*
 * Starts the Bluetooth Low Energy server, registering the services in the DBUS,
 * and starting an infinite loop (main_loop) to attend to data changes sent by
 * the main process.
 */
int ble_server();

#endif /* GATT_SERVICE_H_ */
