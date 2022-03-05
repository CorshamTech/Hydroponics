//****************************************************************************
// Reads a number of sensors and writes the output to a log file.
//
// 02/13/2022 - Bob Applegate, bob@applegate.org
//
// Parts used:
//
// Raspberry Pi, 32 GB micro SD card, power supply
// Sparkfun pHAT (https://www.adafruit.com/product/5142)
// Adafruit PCT2075 temperature sensor (https://www.adafruit.com/product/4369)
// Adafruit 4648 ADC (https://www.adafruit.com/product/4648)
// Grove pH sensor (https://wiki.seeedstudio.com/Grove-PH-Sensor-kit/)
// SHT30 temerature/humidity sensor (https://www.adafruit.com/product/5064)
//
// The pHsensor is available on Amazon and many other sites.
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
#include <time.h>


// How often, in minutes, betweeen each reporting interval.  This can be
// changed on the command line.

#define DEFAULT_REPORTING_INTERVAL	15

// This is the default report file.  Can be changed on the command line.

#define	DEFAULT_REPORT_FILENAME		"/home/pi/Jason/report.csv"

// This is the I2C address of the PCT2075 sensor.  Do not change this unless
// the device is moved to a different address via the address selection bits.

#define PCT2075_ADDR	0x37

// This is the I2C address of the PCF8591 ADC.  Do not change this unless
// the device is moved to a different address via the address selection bits.

#define ADC_ADDR	0x48

// This is the I2C address of the SHT30 sensor.  Do not change this unless
// the device is moved to a different address via the address selection bits.

#define SHT30_ADDR	0x44

// Microseconds in a millisecond

#define US_IN_MS	1000

// The maximum voltage that the pH sensor provides.  It is always 3.3 volts.

#define SENSOR_VOLTAGE	3.3

// Constants for converting voltage into pH.  Beats me what the values mean.

#define CONSTANT	-19.18518519
#define OFFSET		41.02740741		//deviation compensate

static void pollTemp(int fd, FILE *report);
static void pollPH(int fd, FILE *report);
static void pollSHT30(int fd, FILE *report);




//****************************************************************************
int main(int argc, char **argv)
{
	int reportingInterval = DEFAULT_REPORTING_INTERVAL;
	char *reportFilename = (char *)DEFAULT_REPORT_FILENAME;

	// Open the I2C interface

	int i2cfd = open("/dev/i2c-1", O_RDWR);
	if (i2cfd <= 0)
	{
		printf("Error opening I2C device: %s\n", strerror(errno));
		exit(1);
	}

	FILE *report = fopen(reportFilename, "a");
	if (report == NULL)
	{
		printf("Error opening report file %s: %s\n", reportFilename, strerror(errno));
	}
	else
	{
		// If the offset is zero then the file is empty and needs the headers
		// written, otherwise do not write the headers again.

		if (ftell(report) == 0)		// zero means empty file
		{
			fprintf(report, "Date,Time,epoch,");

			// Call each reporting function a negative FD; this tells them to
			// write their column headers to the output file.

			pollTemp(-1, report);
			fprintf(report, ",");		// comma between fields
			pollPH(-1, report);
			fprintf(report, ",");		// comma between fields
			pollSHT30(-1, report);
			fprintf(report, "\n");
			fclose(report);
		}
	}

	// The main loop...

	for (;;)
	{
		FILE *report = fopen(reportFilename, "a");
		if (report == NULL)
		{
			printf("Error opening report file %s: %s\n", reportFilename, strerror(errno));
		}
		else
		{
			time_t now = time(NULL);	// get current time... this is Unix magic
			struct tm *tp = localtime(&now);	// convert to local time

			fprintf(report, "%02d/%02d/%04d,%02d:%02d:%02d,%lu,",
				tp->tm_mon + 1, tp->tm_mday, tp->tm_year + 1900,
				tp->tm_hour, tp->tm_min, tp->tm_sec,
				now);

			pollTemp(i2cfd, report);	// get and display value
			fprintf(report, ",");		// comma between fields

			pollPH(i2cfd, NULL);		// throw out first
			usleep(100 * US_IN_MS);		// delay 100ms
			pollPH(i2cfd, report);		// get actual value
			fprintf(report, ",");		// comma between fields

			pollSHT30(i2cfd, report);
			fprintf(report, "\n");
			fclose(report);
		}

		// Now sleep a while.

//printf("About to sleep %d seconds\n", reportingInterval * 60);
		sleep(reportingInterval * 60);
	}

	exit(0);
}




//****************************************************************************
// This polls the PCT2075 temperatue sensor and prints the value.

static void pollTemp(int fd, FILE *report)
{
	// See if this is a request to write column headers to the output file

	if (fd < 0)
	{
		fprintf(report, "PCT_C,PCT_F");
		return;
	}

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

	unsigned raw = (buffer[0] << 8) | buffer[1];
	float cTemp = raw / 256.0;
	float fTemp = (cTemp * 9.0 / 5.0) + 32;
	fprintf(report, "%1.1f,%1.1f", cTemp, fTemp);
}




//****************************************************************************
// This polls the ADC and returns the value.  Note that the initial read
// will always be 128 (0x80).  Each poll actually returns the previous poll
// value and starts another conversion.

static void pollPH(int fd, FILE *report)
{
	// See if this is a request to write column headers to the output file

	if (fd < 0)
	{
		fprintf(report, "pH");
		return;
	}

	if (ioctl(fd, I2C_SLAVE, ADC_ADDR) < 0)
	{
		printf("Error acquiring sensor: %s\n", strerror(errno));
		return;
	}

	int got;

	unsigned char buffer[8];

	buffer[0] = 0x00;
	buffer[1] = 0x00;
	if ((got = write(fd, buffer, 2)) != 2)
	{
		printf("%d Error writing sensor command: %s\n", got, strerror(errno));
		return;
	}

	if ((got = read(fd, buffer, 4)) != 4)
	{
		printf("Got %d as a return code\n", got);
		perror("Reading sensor data");
		return;
	}

	if (report != NULL)
	{
		// Now convert the raw value into a PH

		float voltage = buffer[0] * (SENSOR_VOLTAGE / 255);
		float ph = CONSTANT * voltage + OFFSET;
		fprintf(report, "%1.1f", ph);
	}
}




//****************************************************************************
// This polls the currently selected SHT30, given the FD to the i2c device.
// Returns either 0 (good) or -1 (bad).  For good conditions this displays
// the temp and himidity.  For bad results, displays an error message.

static void pollSHT30(int fd, FILE *report)
{
	// See if this is a request to write column headers to the output file

	if (fd < 0)
	{
		fprintf(report, "TempC,TempF,Humidity");
		return;
	}

	if (ioctl(fd, I2C_SLAVE, SHT30_ADDR) < 0)
	{
		printf("Error acquiring sensor: %s\n", strerror(errno));
		return;
	}

	unsigned char buffer[2];
	buffer[0] = 0x2c;
	buffer[1] = 0x06;
	if (write(fd, buffer, 2) != 2)
	{
		printf("Error writing sensor command: %s\n", strerror(errno));
		return;
	}

	int got;

	if ((got = read(fd, buffer, 6)) != 6)
	{
		printf("Got %d as a return code\n", got);
		perror("Reading sensor data");
		return;
	}

	float temp = buffer[0] * 256 + buffer[1];
	float cTemp = -45 + (175 * temp / 65536.0);
	float fTemp = -49 + (315 * temp / 65536.0);
	float humidity = 100 * (buffer[3] * 256 + buffer[4]) / 65536.0;

	fprintf(report, "%1.2f,%1.2f,%1.2f%%", cTemp, fTemp, humidity);
}


