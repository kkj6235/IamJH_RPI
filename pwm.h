#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>

int PWMExport(int pwmnum);
int PWMEnable(int pwmnum);
int PWMWritePeriod(int pwmnum, int value);
int PWMWriteDutyCycle(int pwmnum, int value);
