//****************************************************************************
// Reads values from an Adafruit temperature sensor
//
// 02/12/2022 - Bob Applegate, bob@applegate.org
//
// Parts used:
//
// Raspberry Pi, 32 GB micro USB, power supply
// Sparkfun pHAT (https://www.adafruit.com/product/5142)
// Adafruit temperature sensor (https://www.adafruit.com/product/4369)

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


// This is the I2C address of the PCT2075 sensor.  Do not change this unless
// the device is moved to a different address via the address selection bits.

#define PCT2075_ADDR	0x37

static void pollTemp(int fd);




//****************************************************************************
int main(int argc, char **argv)
{
	int i2cfd = open("/dev/i2c-1", O_RDWR);
	if (i2cfd <= 0)
	{
		printf("Error opening I2C device: %s\n", strerror(errno));
		exit(1);
	}

	pollTemp(i2cfd);	// get and display value

	exit(0);
}




//****************************************************************************
// This polls the temperatue sensor and prints the value.

static void pollTemp(int fd)
{
	if (ioctl(fd, I2C_SLAVE, PCT2075_ADDR) < 0)
	{
		printf("Error acquiring sensor: %s\n", strerror(errno));
		return;
	}

	int got;

	unsigned char buffer[8];

	// Send over a request to read from the data register, address 0

	buffer[0] = 0x00;
	if ((got = write(fd, buffer, 1)) != 1)
	{
		printf("%d Error writing sensor command: %s\n", got, strerror(errno));
		return;
	}

	if ((got = read(fd, buffer, 2)) != 2)
	{
		printf("Got %d as a return code\n", got);
		perror("Reading sensor data");
		return;
	}

	// Okay, so I had to play around a bit to get reasonable values, although
	// it does not seem to jive with the datasheet.  Whatever, it works.

	//printf("raw 0x%02X 0x%02X\n", buffer[0], buffer[1]);
	unsigned raw = (buffer[0] << 8) | buffer[1];
	float cTemp = raw / 256.0;
	float fTemp = (cTemp * 9.0 / 5.0) + 32;
	printf("%1.1f C, %1.1f F\n", cTemp, fTemp);
}


