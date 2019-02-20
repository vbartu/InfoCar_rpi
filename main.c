
#include "accerelometer.h"
#include "state_machine/state_machine.h"
#include "ble_server/gatt-service.h"
#include "bt_iface.h"


/*
 * Main process
 */
int main() {

	// Here the second process is created.
	// The child process executes the BLE Server.
	if(fork() == 0) {
		int n = ble_server();
		return n;
	}

	// Configures the interface to communicate between the processes
	// using DBUS.
	set_up_bt_iface();

	// Creates, configures, and starts the state machine.
	fsm_t* state_machine = fsm_new (WAIT_START, trans_table, NULL);

	fsm_setup(state_machine);
	control_setup();


	// Infinite loop of the state machine.
	unsigned int next = millis();
	while (1) {
		fsm_fire (state_machine);
		next += CLK_MS;
		delay_until (next);
	}

	fsm_destroy (state_machine);
}
