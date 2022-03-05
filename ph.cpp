//****************************************************************************
// Reads values from a Grove pH sensor, using a PCF8591 ADC.
//
// 02/12/2022 - Bob Applegate, bob@applegate.org
//
// Parts used:
//
// Raspberry Pi, 32 GB micro USB, power supply
// Sparkfun pHAT (https://www.adafruit.com/product/5142)
// Adafruit 4648 ADC (https://www.adafruit.com/product/4648)
// Grove pH sensor (https://wiki.seeedstudio.com/Grove-PH-Sensor-kit/)
//
// The sensor is available on Amazon and many other sites.
//
// There was also some soldering of a Grove cable to the ADC board.  Cable was
// something like $1.
//
// Big comment on the web site:
//
// Attention
//
//    Before detecting the target solution, the sensor MUST be calibrated by pointed
//    calibration fluid, and it also MUST be put into pointed buffer(PH=7) or clean
//    water before detecting a new kind of solution and swiped.
//    
//    Before being measured, the electrode must be calibrated with a standard buffer
//    solution of known PH value. In order to obtain more accurate results, the
//    known PH value should be reliable, and closer to the measured one.
//    
//    When the measurement is completed, the electrode protective sleeve should be
//    put on. A small amount of 3.3mol / L potassium chloride solution should be
//    placed in the protective sleeve to keep the electrode bulb wet.
//    
//    The leading end of the electrode must be kept clean and dry to absolutely
//    prevent short circuits at both ends of the output, otherwise it will lead to
//    inaccurate or invalid measurement results.
//    
//    After long-term use of the electrode, if you find that the gradient is
//    slightly inaccurate, you can soak the lower end of the electrode in 4% HF
//    (hydrofluoric acid) for 3-5 seconds, wash it with distilled water, and then
//    soak in potassium chloride solution to make it new.
//    
//    The sensor MUST NOT be dipped in the detecting liquid for a long time.

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


// Microseconds in a millisecond

#define US_IN_MS	1000

// This is the I2C address of the PCF8591 ADC.  Do not change this unless
// the device is moved to a different address via the address selection bits.

#define ADC_ADDR	0x48

// The maximum voltage that the pH sensor provides.  It is always 3.3 volts.

#define SENSOR_VOLTAGE	3.3

// Constants for converting voltage into pH.  Beats me what the values mean.

#define CONSTANT	-19.18518519
#define OFFSET		41.02740741		//deviation compensate


static int pollPH(int fd);




//****************************************************************************
int main(int argc, char **argv)
{
	int i2cfd = open("/dev/i2c-1", O_RDWR);
	if (i2cfd <= 0)
	{
		printf("Error opening I2C device: %s\n", strerror(errno));
		exit(1);
	}

	pollPH(i2cfd);			// throw out first
	usleep(100 * US_IN_MS);		// delay 100ms
	int raw = pollPH(i2cfd);	// get actual value

	// Now conver the raw value into a PH

	float voltage = raw * (SENSOR_VOLTAGE / 255);
	printf("raw = %u, voltage = %f\n", raw, voltage);	// handy debugging data
	float ph = CONSTANT * voltage + OFFSET;
	printf("pH = %f\n", ph);

	exit(0);
}




//****************************************************************************
// This polls the ADC and returns the value.  Note that the initial read
// will always be 128 (0x80).  Each poll actually returns the previous poll
// value and starts another conversion.

static int pollPH(int fd)
{
	if (ioctl(fd, I2C_SLAVE, ADC_ADDR) < 0)
	{
		printf("Error acquiring sensor: %s\n", strerror(errno));
		return -1;
	}

	int got;

	unsigned char buffer[8];

	buffer[0] = 0x00;
	buffer[1] = 0x00;
	if ((got = write(fd, buffer, 2)) != 2)
	{
		printf("%d Error writing sensor command: %s\n", got, strerror(errno));
		return -1;
	}

	if ((got = read(fd, buffer, 4)) != 4)
	{
		printf("Got %d as a return code\n", got);
		perror("Reading sensor data");
		return -1;
	}

	return buffer[0];
}


