
#include <wiringPi.h>
#include <wiringSerial.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include "GPS.h"


static const char *device = "/dev/serial0";	// GPS address
static const int baudios = 9600;

int fd;										//file descriptor
int data_num;
int i, j;
char ch[10000];
int commas;
char speed[5]; // 300 km/h is 161 knots, we will never exceed 5 digits,
			   // 3 for integer numbers and 2 for decimals


int32_t get_GPS_speed() {

	int32_t speed = 0;

	fd = serialOpen(device, baudios); //initialize the device
	if (fd != -1) {						//if -1 then error
		data_num = serialDataAvail(fd); //number of available data
		if (data_num == -1) {
			return speed; //if -1 error, then velocity = 0;
		}
		speed = getData(fd); //no error, so we get the velocity
	}
	serialClose(fd);

	return speed;

}

int32_t getData(int fd_aux) {

	for (i = 0; i <= 10000; i++) {
		ch[i] = serialGetchar(fd_aux);

		if (ch[i] == 'V' && ch[i - 1] == 'P') {	//we check that we have reached at least up to $GPVTG to have all the info of $GPRMC

			for (i = 0; i <= 10000; i++) {//we go back to check the cut section

				if (ch[i] == 'R' && ch[i - 1] == 'P') {	//we select the only line with $GPRMC
					commas = 0;
					for (j = i; j <= 10000; j++) {
						if(commas == 7) {	//the speed is the value after the seventh comma
							int aux = 0;
							for (; j <= 10000; j++) {
								if (ch[j] != ',') {	//we check until we find a comma
									speed[aux] = ch[j];
									aux++;
								} else  //when we found a comma we know that we have all the speed information
									return (int32_t) (atof(speed) * 100); //we eliminate decimals and convert to int32
							}
						} else {
							if (ch[j] == ',')	//if we aren't in the seventh comma, comma +1
								commas++;
						}
					}
				}
			}
		}
	}
	return 0;
}





