//****************************************************************************
// This accesses an SHT30 temperature/humidity sensor to display those values.
// (https://www.adafruit.com/product/5064)
//
// Early 2021, Bob Applegate, bob@applegate.org
//
// Devices used:
//
// Raspberry Pi, 32 GB micro USB, power supply
// Sparkfun pHAT (https://www.adafruit.com/product/5142)
// SHT30 temerature/humidity sensor (https://www.adafruit.com/product/5064)

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <linux/i2c-dev.h>
#include <sys/ioctl.h>

// Initially I thought of using multiple of these SHT30 devices so I added
// Sparkfun I2C MUX (multiplexer) https://www.adafruit.com/product/4704.
// In order to support both the MUX and non-MUX, this constant indicates
// whether there is a MUX or not.

#undef USE_MUX
#define FIRST_PORT	0
#define LAST_PORT	3

// Hard coded I2C addresses that should not be changed unless the address
// jumpers on each board are also modified.

#define SHT30_ADDR	0x44
#define MUX_ADDR	0x70

static int selectMUXport(int fd, int port);
static int pollSHT30(int fd);




//****************************************************************************
int main(int argc, char **argv)
{
	int i2cfd = open("/dev/i2c-1", O_RDWR);
	if (i2cfd <= 0)
	{
		printf("Error opening I2C device: %s\n", strerror(errno));
		exit(1);
	}

#ifdef USE_MUX
	// Loop through all the sensors.  Select the port, then get
	// the data from the sensor.

	for (int port = FIRST_PORT; port < LAST_PORT; port++)
	{
		printf("Sensor on port %d: ", port);
		if (selectMUXport(i2cfd, port) == 0)	// if no error with MUX
		{
			pollSHT30(i2cfd);		// poll the sensor
		}
	}
#else	// USE_MUX
	pollSHT30(i2cfd);		// no need to select a port
#endif	// USE_MUX

	exit(0);
}




//****************************************************************************
// Given an open fd to the i2c device and a port number, this selects this
// port on the TCA9548A MUX.  Returns 0 if good, prints an error and returns
// -1 on error.  The port number is 0 to 7.

static int selectMUXport(int fd, int port)
{
	if (ioctl(fd, I2C_SLAVE, MUX_ADDR) < 0)
	{
		printf("Error acquiring MUX device: %s\n", strerror(errno));
		return -1;
	}

	unsigned char data;
	data = 1 << port;
	if (write(fd, &data, 1) != 1)
	{
		printf("Error writing command to MUX: %s\n", strerror(errno));
		return -1;
	}

	return 0;
}




//****************************************************************************
// This polls the currently selected SHT30, given the FD to the i2c device.
// Returns either 0 (good) or -1 (bad).  For good conditions this displays
// the temp and himidity.  For bad results, displays an error message.

static int pollSHT30(int fd)
{

	if (ioctl(fd, I2C_SLAVE, SHT30_ADDR) < 0)
	{
		printf("Error acquiring sensor: %s\n", strerror(errno));
		return -1;
	}

	unsigned char buffer[2];
	buffer[0] = 0x2c;
	buffer[1] = 0x06;
	if (write(fd, buffer, 2) != 2)
	{
		printf("Error writing sensor command: %s\n", strerror(errno));
		return -1;
	}

	int got;

	if ((got = read(fd, buffer, 6)) != 6)
	{
		printf("Got %d as a return code\n", got);
		perror("Reading sensor data");
		return -1;
	}

	float temp = buffer[0] * 256 + buffer[1];
	float cTemp = -45 + (175 * temp / 65536.0);
	float fTemp = -49 + (315 * temp / 65536.0);
	float humidity = 100 * (buffer[3] * 256 + buffer[4]) / 65536.0;

//	printf("Temp: %f C, %f F, humidity %f%%\n", cTemp, fTemp, humidity);
	printf("Temp: %1.2f C, %1.2f F, humidity %1.2f%%\n", cTemp, fTemp, humidity);

	return 0;
}


