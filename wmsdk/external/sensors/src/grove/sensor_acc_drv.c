/*
 * Original: MMA7760.h
 * Library for accelerometer_MMA7760
 *
 * Copyright (c) 2013 seeed technology inc.
 * Author	:	FrankieChu
 * Create Time	:	Jan 2013
 * Change Log	:	Converted to c file, renamed
 *			and updated for AWS sensor integration
 * Changed by	:	Prafulla Wadaskar, Marvell International Ltd.
 *
 * The MIT License (MIT)
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#include <wm_os.h>
#include <wmstdio.h>
#include <wmtime.h>
#include <wmsdk.h>
#include <board.h>
#include <mdev_gpio.h>
#include <mdev_pinmux.h>
#include <lowlevel_drivers.h>

#include "sensor_drv.h"
#include <sensor_acc_drv.h>

#include <wm_os.h>
#include <mdev_i2c.h>

/*------------------Macro Definitions ------------------*/
#define BUF_LEN		16
#define I2C_SLV_ADDR	MMA7660_ADDR 
#define MMA7660TIMEOUT	500	/* us */

/*------------------Global Variables -------------------*/
static mdev_t *i2c0;
static uint8_t read_data[BUF_LEN];
static uint8_t write_data[BUF_LEN];
static struct MMA7660_LOOKUP accLookup[64];

/*
 *********************************************************
 **** Accelerometer Sensor H/W Specific code
 **********************************************************
 */

/*Function: Writes a byte to the register of the MMA7660*/
void MMA7660_write(uint8_t _register, uint8_t _data)
{
	if (!i2c0)
		return;

	write_data[0] = _register;
	write_data[1] = _data;
	i2c_drv_write(i2c0, write_data, 2);
}

/*Function: Writes only register byte of the MMA7660 */
void MMA7660_From(uint8_t _register)
{
	if (!i2c0)
		return;

	write_data[0] = _register;
	i2c_drv_write(i2c0, write_data, 1);
}

/*Function: Read a byte from the regitster of the MMA7660*/
uint8_t MMA7660_read(uint8_t _register)
{
	if (!i2c0)
		return 0;

	write_data[0] = _register;
	i2c_drv_write(i2c0, write_data, 1);
	i2c_drv_read(i2c0, read_data, 1);
	return read_data[0];
}

/* Populate lookup table based on the MMA7660 datasheet at
	http://www.farnell.com/datasheets/1670762.pdf
*/
void MMA7660_initAccelTable() {
	int i;
	float val, valZ;

	for (i = 0, val = 0; i < 32; i++) {
		accLookup[i].g = val;
		val += 0.047;
	}

	for (i = 63, val = -0.047; i > 31; i--) {
		accLookup[i].g = val;
		val -= 0.047;
	}

	for (i = 0, val = 0, valZ = 90; i < 22; i++) {
		accLookup[i].xyAngle = val;
		accLookup[i].zAngle = valZ;

		val += 2.69;
		valZ -= 2.69;
	}

	for (i = 63, val = -2.69, valZ = -87.31; i > 42; i--) {
		accLookup[i].xyAngle = val;
		accLookup[i].zAngle = valZ;

		val -= 2.69;
		valZ += 2.69;
	}

	for (i = 22; i < 43; i++) {
		accLookup[i].xyAngle = 255;
		accLookup[i].zAngle = 255;
	}
}

void MMA7660_setMode(uint8_t mode) {
	MMA7660_write(MMA7660_MODE,mode);
}

void MMA7660_setSampleRate(uint8_t rate) {
	MMA7660_write(MMA7660_SR,rate);
}

void MMA7660_init()
{
	MMA7660_initAccelTable();
	MMA7660_setMode(MMA7660_STAND_BY);
	MMA7660_setSampleRate(AUTO_SLEEP_32);
	/* MMA7660_write(MMA7660_INTSU, interrupts); */
	MMA7660_setMode(MMA7660_ACTIVE);
}

/* Function: Get the contents of the registers in the MMA7660
	so as to calculate the acceleration.
*/
bool MMA7660_getXYZ(int8_t *x,int8_t *y,int8_t *z)
{
	int8_t val[3];
	val[0] = val[1] = val[2] = 64;

	if (!i2c0)
		return 0;

	MMA7660_From(3);
	i2c_drv_read(i2c0, (uint8_t *)val, 3);

	*x = (val[0]<<2)/4;
	*y = (val[1]<<2)/4;
	*z = (val[2]<<2)/4;

	return 1;
}

/* Basic Sensor IO initialization to be done here

	This function will be called only once during sensor registration
 */
int acc_sensor_init(struct sensor_info *curevent)
{
	/* Initialize I2C Driver */
	/* using IO_04:SDA and IO_05 SCL (configured in board file */
	i2c_drv_init(I2C0_PORT);

	/* I2C0 is configured as master */
	i2c0 = i2c_drv_open(I2C0_PORT, I2C_SLAVEADR(I2C_SLV_ADDR));

	/* Initialize Acc Sensor H/W */
	MMA7660_init();

	return 0;
}

/* Sensor input from IO should be read here and to be passed
	in curevent->event_curr_value variable to the upper layer

	This function will be called periodically by the upper layer
	hence you can poll your input here, and there is no need of
	callback IO interrupt, this is very usefull to sense variable
	data from analog sensors connected to ADC lines. Event this can
	be used to digital IO scanning and polling
*/
int MMA7660acc_sensor_input_scan(struct sensor_info *curevent)
{
	int8_t x, y, z;

	if (MMA7660_getXYZ(&x, &y, &z)) {
		dbg("%s Accelerometer:X=%d,Y=%d,Z=%d\r\n",
			__FUNCTION__, x, y, z);

		/* Report newly generated value to the Sensor layer */
		sprintf(curevent->event_curr_value, "%d,%d,%d",
			x, y, z);
		return 0;
	} else
		return -1;
}

struct sensor_info event_MMA7660acc_sensor = {
	.property = "MMA7660-Accelerometer-XYZ",
	.init = acc_sensor_init,
	.read = MMA7660acc_sensor_input_scan,
};

int acc_sensor_event_register(void)
{
	return sensor_event_register(&event_MMA7660acc_sensor);
}
