#include "pwm.h"
#include "gpio.h"

int PWMExport(int pwmnum) {
    char path[VALUE_MAX];
    int fd, byte;

    snprintf(path, VALUE_MAX, "/sys/class/pwm/pwmchip0/export");
    fd = open(path, O_WRONLY);
    if (-1 == fd) {
        fprintf(stderr, "Failed to open export for export!\n");
        return (-1);
    }

    byte = write(fd, "0", 1);  // Use "0" for PWM0
    close(fd);

    sleep(1);

    return (0);
}

int PWMEnable(int pwmnum) {
    char path[VALUE_MAX];
    int fd;

    snprintf(path, VALUE_MAX, "/sys/class/pwm/pwmchip0/pwm0/enable");
    fd = open(path, O_WRONLY);
    if (-1 == fd) {
        fprintf(stderr, "Failed to open in enable!\n");
        return -1;
    }

    write(fd, "1", 1);
    close(fd);

    return (0);
}

int PWMWritePeriod(int pwmnum, int value) {
    char path[VALUE_MAX];
    int fd, byte;

    snprintf(path, VALUE_MAX, "/sys/class/pwm/pwmchip0/pwm0/period");
    fd = open(path, O_WRONLY);
    if (-1 == fd) {
        fprintf(stderr, "Failed to open in period!\n");
        return (-1);
    }

    byte = dprintf(fd, "%d", value);
    close(fd);

    return (0);
}

int PWMWriteDutyCycle(int pwmnum, int value) {
    char path[VALUE_MAX];
    int fd, byte;

    snprintf(path, VALUE_MAX, "/sys/class/pwm/pwmchip0/pwm0/duty_cycle");
    fd = open(path, O_WRONLY);
    if (-1 == fd) {
        fprintf(stderr, "Failed to open in duty cycle!\n");
        return (-1);
    }

    byte = dprintf(fd, "%d", value);
    close(fd);

    return (0);
}
