#ifndef STATE_MACHINE_STATE_MACHINE_H_
#define STATE_MACHINE_STATE_MACHINE_H_

#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <signal.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <sys/time.h>
#include <wiringPi.h>

#include "fsm.h"
#include "../bt_iface.h"

#include "../accerelometer.h"
#include "../thermometer.h"
#include "../GPS.h"

// Flags
#define FLAG_START          	0x01
#define FLAG_STOP           	0x02
#define FLAG_START_WRITE		0x04

#define CLK_MS 250


#define	FLAGS_KEY	       	1
#define	STD_IO_BUFFER_KEY  	2

// States
enum fsm_state {
	GET_DATA_WRITE=2,
	GET_DATA=1,
	WAIT_START=0,
  };

// Input functions
int start(fsm_t* this);
int stop(fsm_t* this);
int start_write(fsm_t* this);

// Output functions
void starting(fsm_t* this);
void starting_write(fsm_t* this);
void set_data(fsm_t* this);
void set_data_write(fsm_t* this);
void stopping(fsm_t* this);
void stopping_write(fsm_t* this);

// Setup functions
int control_setup (void);
void fsm_setup(fsm_t* sm);
PI_THREAD (control_thread);
void delay_until (unsigned int next);

// Transition table
fsm_trans_t trans_table[];

#endif /* STATE_MACHINE_STATE_MACHINE_H_ */
