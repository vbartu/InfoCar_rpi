
#include "state_machine.h"

// Flags variable
volatile int flags = 0;

// File path to the new file, if needed.
char file_path[40];
const char * conf_file_path = "/home/pi/InfoCar/config.txt";
FILE *cf, *wf;

// Stores speed between GPS readings.
int32_t speed = 0;
// Count to read the GPS.
int n_speed = 9;

// Read data.
int32_t data[6];


/*
 * INPUT FUNCTIONS
 * They check if flags is activating them,
 * to change between states.
 */


int start(fsm_t* this) {
	int result;

	piLock(FLAGS_KEY);
	result = (flags & FLAG_START);
	piUnlock(FLAGS_KEY);

	return result;
}

int stop(fsm_t* this) {
	int result;

	piLock(FLAGS_KEY);
	result = (flags & FLAG_STOP);
	piUnlock(FLAGS_KEY);

	return result;
}

int start_write(fsm_t* this) {
	int result;

	piLock(FLAGS_KEY);
	result = (flags & FLAG_START_WRITE);
	piUnlock(FLAGS_KEY);

	return result;
}


/*
 * OUTPUT FUNCTIONS
 */

/*
 * Transition from WAIT_START to GET_DATA.
 */
void starting(fsm_t* this) {
	piLock(STD_IO_BUFFER_KEY);
	printf("\nStarting.\n");
	piUnlock(STD_IO_BUFFER_KEY);
}

/*
 * Transition from WAIT_START to GET_DATA_WRITE.
 * Reads "config.txt" file to set the name of the new file (if
 * it doesn't exist, it is created). Then creates a new file,
 * "raw_dataX.txt", where all the raw collected data will be stored.
 */
void starting_write(fsm_t* this) {
	// Number of "raw_data" files, stored in config.txt
	int n = 0;
	cf = fopen(conf_file_path, "r");
	if (cf != NULL) { // If config.txt exists, reads the number
		fscanf(cf,"%d", &n);
		fclose(cf);
	}
	 // Writes the new number
	cf = fopen("/home/pi/InfoCar/config.txt", "w");
	fprintf(cf, "%d", ++n);
	fclose(cf);

	// Creates the new file
	sprintf(file_path, "/home/pi/InfoCar/raw_data%d.txt", n);
	wf = fopen(file_path, "w");

	piLock(STD_IO_BUFFER_KEY);
	printf("\nStarting.\nCreating new file: %s\n", file_path);
	piUnlock(STD_IO_BUFFER_KEY);


}

/*
 * Transition from GET_DATA to himself.
 * Reads the acceleration and the angular velocity (x and z axis)
 * from the IMU, the temperature from the thermometer, and every
 * nine iterations, the speed from the GPS. Then, it sends
 * this data to the BLE Server.
 */
void set_data(fsm_t* this) {
	setupIMU();
	data[0] = get_x_acceleration();
	data[1] = get_z_acceleration();
	data[2] = get_x_gyro();
	data[3] = get_z_gyro();
	close(2);

	setup_thermometer();
	data[4] = get_temperature();
	close(2);


	if (n_speed++ > 8) {
		n_speed = 0;
		speed = get_GPS_speed();
	}
	data[5] = speed;

	piLock(STD_IO_BUFFER_KEY);
	printf("Acceleration x,z: %d, %d\n", data[0], data[1]);
	printf("Angular velocity x,z: %d, %d\n", data[2], data[3]);
	printf("Temperature: %d\n", data[4]);
	printf("Speed: %d\n\n", data[5]);
	piUnlock(STD_IO_BUFFER_KEY);

	bt_write(data);
}

/*
 * Transition from GET_DATA_WRITE to himself.
 * It does the same as set_data() and then writes the data in
 * the file "raw_dataX.txt" created in starting_write().
 */
void set_data_write(fsm_t* this) {
	setupIMU();
	data[0] = get_x_acceleration();
	data[1] = get_z_acceleration();
	data[2] = get_x_gyro();
	data[3] = get_z_gyro();
	close(2);

	setup_thermometer();
	data[4] = get_temperature();
	close(2);


	if (n_speed++ > 8) {
		n_speed = 0;
		speed = get_GPS_speed();
	}
	data[5] = speed;

	piLock(STD_IO_BUFFER_KEY);
	printf("Acceleration x,z: %d, %d\n", data[0], data[1]);
	printf("Angular velocity x,z: %d, %d\n", data[2], data[3]);
	printf("Temperature: %d\n", data[4]);
	printf("Speed: %d\n\n", data[5]);
	piUnlock(STD_IO_BUFFER_KEY);

	bt_write(data);

	fprintf(wf, "%d %d %d %d %d %d\n", data[0], data[1], data[2], data[3], data[4], data[5]);
}

/*
 * Transition from GET_DATA to WAIT_START.
 */
void stopping(fsm_t* this) {
	piLock(STD_IO_BUFFER_KEY);
	printf("\nFinished.\n");
	piUnlock(STD_IO_BUFFER_KEY);
}

/*
 * Transition from GET_DATA_WRITE to WAIT_START.
 * It closes the file "raw_datai.txt".
 */
void stopping_write(fsm_t* this) {
	fclose(wf);

	piLock(STD_IO_BUFFER_KEY);
	printf("\nFinished. File closed. \n");
	piUnlock(STD_IO_BUFFER_KEY);
}


/*
 * SETUP FUNCTIONS
 */

// Initializes the state machine.
void fsm_setup(fsm_t* sm) {
	piLock(FLAGS_KEY);
	flags = 0;
	piUnlock(FLAGS_KEY);

	piLock(STD_IO_BUFFER_KEY);
	printf("Ready");
	piUnlock(STD_IO_BUFFER_KEY);
}


// Starts the control thread.
int control_setup(void) {
	int x = 0;

	piLock(STD_IO_BUFFER_KEY);

	x = piThreadCreate(control_thread);

	if (x != 0) {
		printf("it didn't start!!!\n");
		piUnlock(STD_IO_BUFFER_KEY);
		return -1;
	}

	piUnlock(STD_IO_BUFFER_KEY);

	return 1;
}

/*
 * Control thread.
 * An infinite loop reads the value of the BLE Server
 * control characteristic every 500 ms, and assigns
 * it to flags. This value is set by the phone.
 */
PI_THREAD (control_thread) {
	// Initial delay to let the BLE Server to start.
	delay(2000);

	while (1) {
		delay(500);

		piLock(FLAGS_KEY);
		flags = bt_read_control();
		piUnlock(FLAGS_KEY);
	}
}

void delay_until(unsigned int next) {
	unsigned int now = millis();

	if (next > now) {
		delay(next - now);
	}
}

// This table contains the transitions between states.
fsm_trans_t trans_table[] = {
					{WAIT_START, start, GET_DATA, starting},
					{WAIT_START, start_write, GET_DATA_WRITE, starting_write},
					{GET_DATA, start, GET_DATA, set_data},
					{GET_DATA, stop, WAIT_START, stopping},
					{GET_DATA_WRITE, start_write, GET_DATA_WRITE, set_data_write},
					{GET_DATA_WRITE, stop, WAIT_START, stopping_write},
					{-1, NULL, -1, NULL},
			};


