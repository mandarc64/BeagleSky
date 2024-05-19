#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <pthread.h>
#include <time.h>

#include <sched.h>

#include "bmp_c.h"

// Define GPIO pin numbers for LCD control
#define LCD_RS  "67"
#define LCD_E   "68"
#define LCD_D4  "44"
#define LCD_D5  "26"
#define LCD_D6  "46"
#define LCD_D7  "65"

pthread_mutex_t lock;
int toggle = 0;
char temp_disp[32], pres_disp[32], ldr_disp[32];

// Forward declarations of functions for GPIO control and sensor management
void unexport_gpio(const char *pin);
void export_gpio(const char *pin);
void set_gpio_direction(const char *pin, const char *direction);
void write_gpio(const char *pin, const char *value);
void lcd_send_nibble(char data);
void lcd_send_byte(char data, int mode);
void lcd_clear();
void lcd_init();
void lcd_print(const char *string);
int read_adc(int channel);
void lcd_display_string(const char *line1, const char *line2);
void close_all(struct bmp280_dev bmp);

// Thread function declarations for sensor reading and display management
void *temp_pressure_thread(void *arg);
void *light_intensity_thread(void *arg);
void *display_thread(void *arg);
void *toggle_thread(void *arg);

int main(int argc, char *argv[]) {
    pthread_t threads[4];
    pthread_attr_t attr;
    struct sched_param param;

    pthread_mutex_init(&lock, NULL);
    lcd_init();

    // Initialize thread attributes and create threads for sensor data processing and display
    pthread_attr_init(&attr);
    param.sched_priority = sched_get_priority_max(SCHED_FIFO);
    pthread_attr_setschedparam(&attr, &param);
    pthread_create(&threads[0], &attr, temp_pressure_thread, NULL);

    pthread_attr_init(&attr);
    param.sched_priority = sched_get_priority_min(SCHED_FIFO);
    pthread_attr_setschedparam(&attr, &param);
    pthread_create(&threads[1], &attr, light_intensity_thread, NULL);

    pthread_attr_init(&attr);
    pthread_create(&threads[2], &attr, display_thread, NULL);
    pthread_create(&threads[3], &attr, toggle_thread, NULL);

    for (int i = 0; i < 4; i++) {
        pthread_join(threads[i], NULL);
    }

    pthread_attr_destroy(&attr);
    pthread_mutex_destroy(&lock);
    return 0;
}

// Function to append data to a CSV file
void append_to_csv(const char *filename, const char *data) {
    FILE *fp = fopen(filename, "a"); // Open the file in append mode
    if (fp == NULL) {
        fprintf(stderr, "Failed to open file for appending\n");
        return;
    }
    time_t now = time(NULL);
    struct tm *t = localtime(&now);
    char date[20];
    strftime(date, sizeof(date), "%Y-%m-%d %H:%M:%S", t); // Format time as YYYY-MM-DD HH:MM:SS

    fprintf(fp, "%s,%s\n", date, data); // Append the timestamp and data to the file
    fclose(fp);
}

// Thread to read temperature and pressure using BMP280 sensor
void *temp_pressure_thread(void *arg) {
    struct bmp280_dev bmp;
    struct bmp280_config conf;
    struct bmp280_uncomp_data ucomp_data;
    double pres, temp;

    conf.filter = BMP280_FILTER_OFF;
    conf.os_temp = BMP280_OS_2X;
    conf.os_pres = BMP280_OS_16X;
    conf.odr = BMP280_ODR_0_5_MS;

    bmp_init(&bmp, &conf, &ucomp_data, USE_I2C);

    while (1) {
        get_values(&temp, &pres, &bmp, &ucomp_data);

        pthread_mutex_lock(&lock);
        sprintf(temp_disp, "Temp: %.2f C", temp);
        sprintf(pres_disp, "Pres: %.2f hPa", pres / 100);
        pthread_mutex_unlock(&lock);
        
         char csv_line[64];
        sprintf(csv_line, "Temperature,%.2f,Pressure,%.2f", temp, pres / 100);
        append_to_csv("sensor_data.csv", csv_line);

        usleep(500000); // Sleep for 500 ms
    }
    return NULL;
}

// Thread to read light intensity using an ADC
void *light_intensity_thread(void *arg) {
    int ldr_value;

    while (1) {
        ldr_value = read_adc(0);

        pthread_mutex_lock(&lock);
        sprintf(ldr_disp, "Light: %d", ldr_value);
        pthread_mutex_unlock(&lock);
        
        char csv_line[64];
        sprintf(csv_line, "Light,%d", ldr_value);
        append_to_csv("sensor_data.csv", csv_line);


        usleep(1000000); // Sleep for 1 second
    }
    return NULL;
}

// Thread to update the LCD display based on sensor values
void *display_thread(void *arg) {
    while (1) {
        pthread_mutex_lock(&lock);
        if (toggle % 2 == 0) {
            lcd_display_string(temp_disp, pres_disp);
        } else {
            lcd_display_string(ldr_disp, pres_disp);
        }
        pthread_mutex_unlock(&lock);
        usleep(1000000);
    }
    return NULL;
}

// Thread to toggle the display between different sensor values
void *toggle_thread(void *arg) {
    while (1) {
        pthread_mutex_lock(&lock);
        toggle++;
        pthread_mutex_unlock(&lock);
        sleep(1);
    }
    return NULL;
}

// Function to unexport a GPIO pin, making it available for other uses
void unexport_gpio(const char *pin) {
    int fd = open("/sys/class/gpio/unexport", O_WRONLY);
    if (fd < 0) {
        fprintf(stderr, "Error: unable to unexport GPIO%s\n", pin);
        return;
    }
    write(fd, pin, strlen(pin));
    close(fd);
}

// Function to export a GPIO pin for use
void export_gpio(const char *pin) {
    int fd;
    char path[50];
    sprintf(path, "/sys/class/gpio/gpio%s", pin);
    if (access(path, F_OK) == -1) { // GPIO not exported
        fd = open("/sys/class/gpio/export", O_WRONLY);
        if (fd < 0) {
            fprintf(stderr, "Error: unable to export GPIO%s\n", pin);
            return;
        }
        write(fd, pin, strlen(pin));
        close(fd);
    } else { // GPIO already exported, try to unexport first
        unexport_gpio(pin);
        fd = open("/sys/class/gpio/export", O_WRONLY);
        if (fd < 0) {
            fprintf(stderr, "Error: unable to export GPIO%s again\n", pin);
            return;
        }
        write(fd, pin, strlen(pin));
        close(fd);
    }
}

// Set the direction of a GPIO pin (input or output)
void set_gpio_direction(const char *pin, const char *direction) {
    char path[50];
    sprintf(path, "/sys/class/gpio/gpio%s/direction", pin);
    int fd = open(path, O_WRONLY);
    if (fd < 0) {
        fprintf(stderr, "Error: unable to set direction for GPIO%s\n", pin);
        return;
    }
    write(fd, direction, strlen(direction));
    close(fd);
}

// Write a high or low value to a GPIO pin
void write_gpio(const char *pin, const char *value) {
    char path[50];
    sprintf(path, "/sys/class/gpio/gpio%s/value", pin);
    int fd = open(path, O_WRONLY);
    if (fd < 0) {
        fprintf(stderr, "Error: opening GPIO%s\n", pin);
        return;
    }
    write(fd, value, strlen(value));
    close(fd);
}

// Initialize the LCD by setting up GPIOs and configuring the display
void lcd_init() {
    export_gpio(LCD_RS);
    export_gpio(LCD_E);
    export_gpio(LCD_D4);
    export_gpio(LCD_D5);
    export_gpio(LCD_D6);
    export_gpio(LCD_D7);

    set_gpio_direction(LCD_RS, "out");
    set_gpio_direction(LCD_E, "out");
    set_gpio_direction(LCD_D4, "out");
    set_gpio_direction(LCD_D5, "out");
    set_gpio_direction(LCD_D6, "out");
    set_gpio_direction(LCD_D7, "out");

    usleep(15000); // Wait more than 15ms after Vcc rises to 4.5V
    lcd_send_byte(0x03, 0); // Function Set
    usleep(4100);  // Wait more than 4.1ms
    lcd_send_byte(0x03, 0); // Function Set
    usleep(100);   // Wait more than 100μs
    lcd_send_byte(0x03, 0); // Function Set
    lcd_send_byte(0x02, 0); // Function Set to switch to 4-bit mode

    // Set # lines, font size, etc.
    lcd_send_byte(0x28, 0); // Function Set: 4-bit mode, 2 lines, 5x8 dots
    lcd_send_byte(0x0C, 0); // Display ON, Cursor OFF, Blink OFF
    lcd_send_byte(0x06, 0); // Entry mode: Increment cursor, No display shift
    lcd_clear();            // Clear screen
}

// Send a byte of data to the LCD in 4-bit mode
void lcd_send_byte(char data, int mode) {
    write_gpio(LCD_RS, mode ? "1" : "0");
    lcd_send_nibble(data >> 4);
    lcd_send_nibble(data & 0xF);
}

// Send a nibble (4 bits) to the LCD
void lcd_send_nibble(char data) {
    write_gpio(LCD_D4, (data & 0x1) ? "1" : "0");
    write_gpio(LCD_D5, (data & 0x2) ? "1" : "0");
    write_gpio(LCD_D6, (data & 0x4) ? "1" : "0");
    write_gpio(LCD_D7, (data & 0x8) ? "1" : "0");
    write_gpio(LCD_E, "1");
    usleep(1); // Enable pulse must be >450ns
    write_gpio(LCD_E, "0");
    usleep(37); // Commands need >37μs to settle
}

// Function to clear the LCD display
void lcd_clear() {
    lcd_send_byte(0x01, 0); // Clear display
    usleep(1520); // Clearing the display takes 1.52ms
}

// Function to print a string on the LCD
void lcd_print(const char *string) {
    while (*string) {
        lcd_send_byte((char)(*string++), 1);
    }
}

// Function to read the value from an ADC channel
int read_adc(int channel) {
    char buf[64];
    char val[16];
    int fd;
    int value;

    snprintf(buf, sizeof(buf), "/sys/bus/iio/devices/iio:device0/in_voltage%d_raw", channel);
    
    fd = open(buf, O_RDONLY);
    if (fd < 0) {
        perror("Failed to open ADC:");
        return -1;
    }

    if (read(fd, val, sizeof(val)) < 0) {
        perror("Failed to read value:");
        close(fd);
        return -1;
    }

    value = atoi(val);
    close(fd);

    return value;
}


// Function to display two strings on the LCD (one on each line)
void lcd_display_string(const char *line1, const char *line2) {
    lcd_clear();
    lcd_print(line1);
    lcd_send_byte(0xC0, 0); // Move cursor to second line
    lcd_print(line2);
}
