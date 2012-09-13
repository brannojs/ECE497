#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <poll.h>
#include <signal.h>
#include "i2c-dev.h"
#include <poll.h>
#include <signal.h>
#include <unistd.h>
#include <errno.h>

/****************************************************************
 * Constants
 ****************************************************************/
#define SYSFS_GPIO_DIR "/sys/class/gpio"
#define POLL_TIMEOUT (3 * 1000) /* 3 seconds */
#define MAX_BUF 64

int keepgoing = 1;         

void signal_handler(int sig)
{
        printf( "Program Exited by User\n" );
        keepgoing = 0;
}


int main()
{
	//Stop program if Ctrl - C is pressed
	signal(SIGINT, signal_handler);

	//prepare gpio input used for interrupts
	struct pollfd fdset[2];
	int nfds = 2;
	int gpio_fd, timeout, rc;
	char *buf[MAX_BUF];
	unsigned int gpio;
	int len;
	gpio = 31;
	gpio_export(gpio);
	gpio_set_dir(gpio, 0);
	gpio_set_edge(gpio, "rising");
	gpio_fd = gpio_fd_open(gpio);
	timeout = POLL_TIMEOUT;

	//prepare gpio for gpio output
	export_gpio(3);

	//prepare for i2c communication
	int i2cbus, address, dataadd, temp, file, size;
	i2cbus = 3;
	address = 75;
	dataadd = 0;
	size = I2C_SMBUS_BYTE;
	char filename[20];
	sprintf(filename, "/dev/i2c-%d", i2cbus);

	//read voltage on ain3 (power to temp. sensor) to see if power is provided
	int voltage = read_ain("ain3");
	if(voltage < 3000)
		printf("Temp Sensor Vdd is too low.\n");
	else
		printf("Temp Sensor Vdd is sufficient.\n");

	//continue setting up gpio for output
	set_gpio_direction(03,"out");
	set_gpio_value(03,1);

	//set up pwm
	set_mux_value("gpmc_a2",6);
	set_pwm("ehrpwm.0:0",1,50);

	while(keepgoing)
	{
		//interrupt code
		memset((void*)fdset, 0, sizeof(fdset));

		fdset[0].fd = STDIN_FILENO;
		fdset[0].events = POLLIN;
      
		fdset[1].fd = gpio_fd;
		fdset[1].events = POLLPRI;

		rc = poll(fdset, nfds, timeout);   
		
		if (fdset[1].revents & POLLPRI) {
			//runs when interrupt occurs
			len = read(fdset[1].fd, buf, MAX_BUF);
			//get temp. from sensor and print to console
			 file = open(filename, O_RDWR);
       			 ioctl(file, I2C_SLAVE, address);
      			 temp = i2c_smbus_read_byte_data(file,dataadd);
		       	 close(file);
			printf("Current Temperature: %d\n",temp);
			if(temp < 26)
				set_gpio_value(03,0);
			else 	
				set_gpio_value(03,1);
		}
	fflush(stdout);
	}
	
	gpio_fd_close(gpio_fd);
	return 0;
}


/****************************************************************
 * gpio_export
 ****************************************************************/
int gpio_export(unsigned int gpio)
{
	int fd, len;
	char buf[MAX_BUF];
 
	fd = open(SYSFS_GPIO_DIR "/export", O_WRONLY);
	if (fd < 0) {
		perror("gpio/export");
		return fd;
	}
 
	len = snprintf(buf, sizeof(buf), "%d", gpio);
	write(fd, buf, len);
	close(fd);
 
	return 0;
}

/****************************************************************
 * gpio_unexport
 ****************************************************************/
int gpio_unexport(unsigned int gpio)
{
	int fd, len;
	char buf[MAX_BUF];
 
	fd = open(SYSFS_GPIO_DIR "/unexport", O_WRONLY);
	if (fd < 0) {
		perror("gpio/export");
		return fd;
	}
 
	len = snprintf(buf, sizeof(buf), "%d", gpio);
	write(fd, buf, len);
	close(fd);
	return 0;
}

/****************************************************************
 * gpio_set_dir
 ****************************************************************/
int gpio_set_dir(unsigned int gpio, unsigned int out_flag)
{
	int fd, len;
	char buf[MAX_BUF];
 
	len = snprintf(buf, sizeof(buf), SYSFS_GPIO_DIR  "/gpio%d/direction", gpio);
 
	fd = open(buf, O_WRONLY);
	if (fd < 0) {
		perror("gpio/direction");
		return fd;
	}
 
	if (out_flag)
		write(fd, "out", 4);
	else
		write(fd, "in", 3);
 
	close(fd);
	return 0;
}

/****************************************************************
 * gpio_set_value
 ****************************************************************/
int gpio_set_value(unsigned int gpio, unsigned int value)
{
	int fd, len;
	char buf[MAX_BUF];
 
	len = snprintf(buf, sizeof(buf), SYSFS_GPIO_DIR "/gpio%d/value", gpio);
 
	fd = open(buf, O_WRONLY);
	if (fd < 0) {
		perror("gpio/set-value");
		return fd;
	}
 
	if (value)
		write(fd, "1", 2);
	else
		write(fd, "0", 2);
 
	close(fd);
	return 0;
}

/****************************************************************
 * gpio_get_value
 ****************************************************************/
int gpio_get_value(unsigned int gpio, unsigned int *value)
{
	int fd, len;
	char buf[MAX_BUF];
	char ch;

	len = snprintf(buf, sizeof(buf), SYSFS_GPIO_DIR "/gpio%d/value", gpio);
 
	fd = open(buf, O_RDONLY);
	if (fd < 0) {
		perror("gpio/get-value");
		return fd;
	}
 
	read(fd, &ch, 1);

	if (ch != '0') {
		*value = 1;
	} else {
		*value = 0;
	}
 
	close(fd);
	return 0;
}


/****************************************************************
 * gpio_set_edge
 ****************************************************************/

int gpio_set_edge(unsigned int gpio, char *edge)
{
	int fd, len;
	char buf[MAX_BUF];

	len = snprintf(buf, sizeof(buf), SYSFS_GPIO_DIR "/gpio%d/edge", gpio);
 
	fd = open(buf, O_WRONLY);
	if (fd < 0) {
		perror("gpio/set-edge");
		return fd;
	}
 
	write(fd, edge, strlen(edge) + 1); 
	close(fd);
	return 0;
}

/****************************************************************
 * gpio_fd_open
 ****************************************************************/

int gpio_fd_open(unsigned int gpio)
{
	int fd, len;
	char buf[MAX_BUF];

	len = snprintf(buf, sizeof(buf), SYSFS_GPIO_DIR "/gpio%d/value", gpio);
 
	fd = open(buf, O_RDONLY | O_NONBLOCK );
	if (fd < 0) {
		perror("gpio/fd_open");
	}
	return fd;
}

/****************************************************************
 * gpio_fd_close
 ****************************************************************/

int gpio_fd_close(int fd)
{
	return close(fd);
}

/******************************************************************
******************************************************************/
int read_ain(char* ain){
FILE *fp;
char path[MAX_BUF];
char buf[MAX_BUF];

snprintf(path, sizeof path, "/sys/devices/platform/omap/tsc/%s", ain);

if((fp = fopen(path, "r")) == NULL){
printf("Cannot open specified ain pin, %s\n", ain);
return 1;
}

if(fgets(buf, MAX_BUF, fp) == NULL){
printf("Cannot read specified ain pin, %s\n", ain);
}

fclose(fp);
return atoi(buf);	
}

/****************************************************************
* set_gpio_value
****************************************************************/

int set_gpio_value(int gpio, int value){
FILE *fp;
char path[MAX_BUF];

//set value only if direction is out
snprintf(path, sizeof path, "/sys/class/gpio/gpio%d/value", gpio);

if((fp = fopen(path, "w")) == NULL){
printf("Cannot open specified value file.\n", gpio);
return 1;
}

//write 1 or 0 to value file
rewind(fp);
fprintf(fp, "%d\n", value);
fflush(fp);
fclose(fp);
}


/****************************************************************
* set_gpio_direction
****************************************************************/

int set_gpio_direction(int gpio, char* direction){
FILE *fp;
char path[MAX_BUF];

//create path using specified gpio
snprintf(path, sizeof path,"/sys/class/gpio/gpio%d/direction", gpio);
//open direction file
if((fp = fopen(path, "w")) == NULL){
printf("Cannot open specified direction file. Is gpio%d exported?\n", 
gpio);
return 1;
}

//write "in" or "out" to direction file
rewind(fp);
fprintf(fp, "%s\n", direction);
fflush(fp);
fclose(fp);
}


/****************************************************************
* export_gpio
****************************************************************/

int export_gpio(int gpio){
FILE *fp;

//open the export file
if((fp = fopen("/sys/class/gpio/export", "ab")) == NULL){
printf("Cannot open export file. \n");
return 1;
}

//write specified gpio to export file
fprintf(fp, "%d\n", gpio);
fflush(fp);
fclose(fp);

//return 0 if everything runs correctly
return 0;
}



/****************************************************************
* set_pwm
****************************************************************/	
int set_pwm(char* pwm, int period_freq, int duty_percent){
FILE *fp;
char path[MAX_BUF];

snprintf(path, sizeof path, "/sys/class/pwm/%s/run", pwm);

if((fp = fopen(path, "w")) == NULL){
printf("Cannot open pwm run file, %s\n", path);
return 1;
}

rewind(fp);
fprintf(fp, "1\n");
fflush(fp);
fclose(fp);

snprintf(path, sizeof path, "/sys/class/pwm/%s/duty_ns", pwm);

if((fp = fopen(path, "w")) == NULL){
printf("Cannot open pwm duty_ns file, %s\n", path);
}

rewind(fp);
fprintf(fp, "0\n");
fflush(fp);
fclose(fp);

snprintf(path, sizeof path, "/sys/class/pwm/%s/period_freq", pwm);

if((fp = fopen(path, "w")) == NULL){
printf("Cannot open pwm period_freq file, %s\n", path);
}

rewind(fp);
fprintf(fp, "%d\n", period_freq);
fflush(fp);
fclose(fp);

snprintf(path, sizeof path, "/sys/class/pwm/%s/duty_percent", pwm);

if((fp = fopen(path, "w")) == NULL){
printf("Cannot open duty_percent file, %s\n", path);
}

rewind(fp);
fprintf(fp, "%d\n", duty_percent);
fflush(fp);
fclose(fp);
}



/****************************************************************
* set_mux_value
****************************************************************/
int set_mux_value(char* mux, int value){
FILE *fp;
char path[MAX_BUF];

snprintf(path, sizeof path, "/sys/kernel/debug/omap_mux/%s", mux);

if((fp = fopen(path, "w")) == NULL){
printf("Cannot open specified mux, %s\n", mux);
return 1;
}

rewind(fp);
fprintf(fp, "%d\n", value);
fflush(fp);
fclose(fp);

}
