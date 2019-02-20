#ifndef ADVERTISING_H_
#define ADVERTISING_H_

void ad_register(DBusConnection *conn, GDBusProxy *manager, const char *type);
void ad_unregister(DBusConnection *conn, GDBusProxy *manager);

void ad_advertise_uuids(const char *arg);
void ad_advertise_service(const char *arg);
void ad_advertise_manufacturer(const char *arg);
void ad_advertise_tx_power(gboolean value);

#endif /* ADVERTISING_H_ */
