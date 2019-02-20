#ifndef BT_IFACE_H_
#define BT_IFACE_H_

// setup
void set_up_bt_iface();

// write to server
void bt_write(int32_t data[]);

// read control from server
int32_t bt_read_control();


#endif /* BT_IFACE_H_ */
