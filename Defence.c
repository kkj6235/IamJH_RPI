/*
TODO
1. functions for managing GPIOs(LED)
2. Joystic control logic
3. if damaged, zi- sound(within 1.) - servo motor
4. socket programming with other moduls
*/
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <time.h>
#include <arpa/inet.h>
#include <string.h>
#include <sys/socket.h>
#include <pthread.h>

#include <linux/spi/spidev.h> // Can i use?
#include <linux/types.h>
#include <stdint.h>
#include <sys/ioctl.h>
#include <math.h>

#define IN 0
#define OUT 1

#define LOW 0
#define HIGH 1

/*PWM-------------------------------------------------------*/
#define PWM 0
#define VALUE_MAX_PWM 256
#define DIRECTION_MAX_PWM 256

/*GPIO-------------------------------------------------------*/
#define LED_1 17
#define LED_2 27
#define LED_3 22
#define VALUE_MAX_GPIO 40
#define DIRECTION_MAX_GPIO 120
#define BUFFER_MAX 3
#define VALUE_MAX 40
#define ARRAY_SIZE(a) (sizeof(a) / sizeof((a)[0]))

/*JOYSTICK-------------------------------------------------------*/
int joy_fd = 0;
static const char *DEVICE = "/dev/spidev0.0";
static uint8_t MODE = 0;
static uint8_t BITS = 8;
static uint32_t CLOCK = 1000000;
static uint16_t DELAY = 5;

char msg[32] = {0};
int is_exit = 0;

static int PWMExport(int pwmnum) {
#define BUFFER_MAX 3
  char buffer[BUFFER_MAX];
  int fd, byte;

  // TODO: Enter the export path.
  fd = open("/sys/class/pwm/pwmchip0/export", O_WRONLY);
  if (-1 == fd) {
    fprintf(stderr, "Failed to open export for export!\n");
    return (-1);
  }

  byte = snprintf(buffer, BUFFER_MAX, "%d", pwmnum);
  write(fd, buffer, byte);
  close(fd);

  sleep(1);

  return (0);
}

static int PWMEnable(int pwmnum) {
  static const char s_enable_str[] = "1";

  char path[DIRECTION_MAX_PWM];
  int fd;

  // TODO: Enter the enable path.
  snprintf(path, DIRECTION_MAX_PWM, "/sys/class/pwm/pwmchip0/pwm0/enable", pwmnum);
  fd = open(path, O_WRONLY);
  if (-1 == fd) {
    fprintf(stderr, "Failed to open in enable!\n");
    return -1;
  }

  write(fd, s_enable_str, strlen(s_enable_str));
  close(fd);

  return (0);
}

static int PWMWritePeriod(int pwmnum, int value) {
  char s_value_str[VALUE_MAX_PWM];
  char path[VALUE_MAX_PWM];
  int fd, byte;

  // TODO: Enter the period path.
  snprintf(path, VALUE_MAX_PWM, "/sys/class/pwm/pwmchip0/pwm0/period", pwmnum);
  fd = open(path, O_WRONLY);
  if (-1 == fd) {
    fprintf(stderr, "Failed to open in period!\n");
    return (-1);
  }
  byte = snprintf(s_value_str, VALUE_MAX_PWM, "%d", value);

  if (-1 == write(fd, s_value_str, byte)) {
    fprintf(stderr, "Failed to write value in period!\n");
    close(fd);
    return -1;
  }
  close(fd);

  return (0);
}

static int PWMWriteDutyCycle(int pwmnum, int value) {
  char s_value_str[VALUE_MAX_PWM];
  char path[VALUE_MAX_PWM];
  int fd, byte;

  snprintf(path, VALUE_MAX_PWM, "/sys/class/pwm/pwmchip0/pwm0/duty_cycle", pwmnum);
  fd = open(path, O_WRONLY);
  if (-1 == fd) {
    fprintf(stderr, "Failed to open in duty cycle!\n");
    return (-1);
  }
  byte = snprintf(s_value_str, VALUE_MAX_PWM, "%d", value);

  if (-1 == write(fd, s_value_str, byte)) {
    fprintf(stderr, "Failed to write value in duty cycle!\n");
    close(fd);
    return -1;
  }
  close(fd);

  return (0);
}

static int prepare(int fd) {
  if (ioctl(fd, SPI_IOC_WR_MODE, &MODE) == -1) {
    perror("Can't set MODE");
    return -1;
  }

  if (ioctl(fd, SPI_IOC_WR_BITS_PER_WORD, &BITS) == -1) {
    perror("Can't set number of BITS");
    return -1;
  }

  if (ioctl(fd, SPI_IOC_WR_MAX_SPEED_HZ, &CLOCK) == -1) {
    perror("Can't set write CLOCK");
    return -1;
  }

  if (ioctl(fd, SPI_IOC_RD_MAX_SPEED_HZ, &CLOCK) == -1) {
    perror("Can't set read CLOCK");
    return -1;
  }

  return 0;
}

uint8_t control_bits_differential(uint8_t channel) {
  return (channel & 7) << 4;
}

uint8_t control_bits(uint8_t channel) {
  return 0x8 | control_bits_differential(channel);
}

int readadc(int fd, uint8_t channel) {
  uint8_t tx[] = {1, control_bits(channel), 0};
  uint8_t rx[3];

  struct spi_ioc_transfer tr = {
      .tx_buf = (unsigned long)tx,
      .rx_buf = (unsigned long)rx,
      .len = ARRAY_SIZE(tx),
      .delay_usecs = DELAY,
      .speed_hz = CLOCK,
      .bits_per_word = BITS,
  };

  if (ioctl(fd, SPI_IOC_MESSAGE(1), &tr) == 1) {
    perror("IO Error");
    abort();
  }

  return ((rx[1] << 8) & 0x300) | (rx[2] & 0xFF);
}

static int GPIOExport(int pin) {
  char buffer[BUFFER_MAX];
  ssize_t bytes_written;
  int fd;

  fd = open("/sys/class/gpio/export", O_WRONLY);
  if (-1 == fd) {
    fprintf(stderr, "Failed to open export for writing!\n");
    return (-1);
  }

  bytes_written = snprintf(buffer, BUFFER_MAX, "%d", pin);
  write(fd, buffer, bytes_written);
  close(fd);
  return (0);
}

static int GPIOUnexport(int pin) {
  char buffer[BUFFER_MAX];
  ssize_t bytes_written;
  int fd;

  fd = open("/sys/class/gpio/unexport", O_WRONLY);
  if (-1 == fd) {
    fprintf(stderr, "Failed to open unexport for writing!\n");
    return (-1);
  }

  bytes_written = snprintf(buffer, BUFFER_MAX, "%d", pin);
  write(fd, buffer, bytes_written);
  close(fd);
  return (0);
}

static int GPIODirection(int pin, int dir) {
  static const char s_directions_str[] = "in\0out";

  char path[DIRECTION_MAX_GPIO];
  int fd;

  snprintf(path, DIRECTION_MAX_GPIO, "/sys/class/gpio/gpio%d/direction", pin);
  fd = open(path, O_WRONLY);
  if (-1 == fd) {
    fprintf(stderr, "Failed to open gpio direction for writing!\n");
    return (-1);
  }

  if (-1 ==
      write(fd, &s_directions_str[IN == dir ? 0 : 3], IN == dir ? 2 : 3)) {
    fprintf(stderr, "Failed to set direction!\n");
    return (-1);
  }

  close(fd);
  return (0);
}

static int GPIOWrite(int pin, int value) {
  static const char s_values_str[] = "01";

  char path[VALUE_MAX_GPIO];
  int fd;

  snprintf(path, VALUE_MAX_GPIO, "/sys/class/gpio/gpio%d/value", pin);
  fd = open(path, O_WRONLY);
  if (-1 == fd) {
    fprintf(stderr, "Failed to open gpio value for writing!\n");
    return (-1);
  }

  if (1 != write(fd, &s_values_str[LOW == value ? 0 : 1], 1)) {
    fprintf(stderr, "Failed to write value!\n");
    return (-1);
  }

  close(fd);
  return (0);
}

static int GPIORead(int pin) {
  char path[VALUE_MAX_GPIO];
  char value_str[3];
  int fd;

  snprintf(path, VALUE_MAX_GPIO, "/sys/class/gpio/gpio%d/value", pin);
  fd = open(path, O_RDONLY);
  if (-1 == fd) {
    fprintf(stderr, "Failed to open gpio value for reading!\n");
    return (-1);
  }

  if (-1 == read(fd, value_str, 3)) {
    fprintf(stderr, "Failed to read value!\n");
    return (-1);
  }

  close(fd);

  return (atoi(value_str));
}

static int setting()
{
    if (GPIOExport(LED_1) == -1 || GPIOExport(LED_2) == -1 || GPIOExport(LED_3) == -1) {
        printf("GPIOExport Error\n");
    }
    
    usleep(1000*1000);

    if (GPIODirection(LED_1, OUT) == -1 || GPIODirection(LED_2, OUT) == -1 || GPIODirection(LED_3, OUT) == -1) {
        printf("GPIOExport Direction\n");
    }
//Joystic setting---------------------------------------------------------------------
    joy_fd = open(DEVICE, O_RDWR);
    if (joy_fd <= 0) {
        perror("Device open error");
        pthread_exit(0);
    }

    if (prepare(joy_fd) == -1) {
        perror("Device prepare error");
        return -1;
    }

    PWMExport(PWM);
    PWMWritePeriod(PWM, 5000000);
    PWMWriteDutyCycle(PWM, 0);
    PWMEnable(PWM);
}

void error_handling(char *message) {
  fputs(message, stderr);
  fputc('\n', stderr);
  exit(1);
}

void servo()
{
    for (int i = 0; i < 30; i++) {
    PWMWriteDutyCycle(PWM, i * 100000);
    usleep(10000);
    }
    for (int i = 30; i > 0; i--) {
        PWMWriteDutyCycle(PWM, i * 100000);
        usleep(10000);
    }

    for (int i = 0; i < 30; i++) {
    PWMWriteDutyCycle(PWM, i * 100000);
    usleep(10000);
    }
    for (int i = 30; i > 0; i--) {
        PWMWriteDutyCycle(PWM, i * 100000);
        usleep(10000);
    }
}

void * heart(void* clnt_sock)
{
    /*TODO
    1. Socket read, If the val is 1, GPIO out of light,(1,2,3)순
    2. if led_3 is out of light, query the die signal to the server
    */

    int* sock = (int *)clnt_sock;
    int str_len;
    int count = 0;

    while(1)
    {
        str_len = read(*sock,msg,sizeof(msg));
        if(str_len==-1) error_handling("read() error");

        if(strncmp(msg,"1",1)==0)
        {
            count++;
            if(count == 1)
            {

                printf("First heart attack!\n");
                GPIOWrite(LED_1, 0);
                servo();
            }

            if(count == 2)
            {
                printf("Second heart attack!\n");
                GPIOWrite(LED_2, 0);
                servo();
            }

            if(count == 3)
            {
                printf("Third heart attack!\n");
                GPIOWrite(LED_3, 0);
                servo();
                is_exit = 1;

                if (write(*sock,"-1",sizeof("-1"))==-1)
                {
                    printf("Write dead signal error");
                }
            }
        }
        else if(strncmp(msg,"9999",4)==0){
            printf("The process got the dead sign.\n");
            GPIOWrite(LED_1, 0);
            GPIOWrite(LED_2, 0);
            GPIOWrite(LED_3, 0);
            is_exit = 1;
            pthread_exit(0);
        }
    }
}

void* joystick(void* clnt_sock)
{
    /*TODO
    1. send data
    */
    int* sock = (int *)clnt_sock;
    int x_val = 0;
    int y_val = 0;
    int val = 0;
    int std = 30;
    char str[5];

    while (1) {
        x_val = readadc(joy_fd, 0);
        usleep(10000);
        y_val = readadc(joy_fd, 1);

//Value determine----------------------------------------------
        if(x_val>y_val)
        {
            if(x_val>500) val = 1;
            else if(x_val>300) val = 4;
        }
        else
        {
            if(y_val>500) val = 3;
            else if(y_val>220) val = 2;
        }


        sprintf(str,"%d",val);
        printf("joystick output : %d\n", val);

        if (write(*sock,str,sizeof(str))==-1)
        {
            printf("Write Joystick error");
        }
        val = 0;

        if(is_exit) pthread_exit(0);
        usleep(10000);

    }
}

int main(int argc, char *argv[]) {
  int clnt_sock;
  struct sockaddr_in serv_addr;
  char on[2] = "1";
  int status;
  pthread_t p_thread[2];
  int thr_id;
  int* sock_ptr = malloc(sizeof(int));
/*Socket connecting-----------------------------------------------------------*/
  if (argc != 3) {
    printf("Usage : %s <IP> <port>\n", argv[0]);
    exit(1);
  }

  clnt_sock = socket(PF_INET, SOCK_STREAM, 0);
  if (clnt_sock == -1) error_handling("socket() error");

  *sock_ptr = clnt_sock; //For Thread Input

  memset(&serv_addr, 0, sizeof(serv_addr));
  serv_addr.sin_family = AF_INET;
  serv_addr.sin_addr.s_addr = inet_addr(argv[1]);
  serv_addr.sin_port = htons(atoi(argv[2]));

  if (connect(clnt_sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) == -1)
    error_handling("connect() error");

  printf("Connection established\n");
/*-----------------------------------------------------------------------*/
//game start
while(1){
    while(1)
    {
      int bytes_read = read(clnt_sock,msg,sizeof(msg));
      if (bytes_read < 0) {
          perror("Read error");
          exit(EXIT_FAILURE);
      }
      printf("%s\n",msg);
      if(strncmp(msg,"1",1)==0)
      {
          printf("GAME START\n");
          break;
      }
    }
    /*Initialization--------------------------------------------------------------------------------------*/
    setting();
    if (GPIOWrite(LED_1, 1) == -1 || GPIOWrite(LED_2, 1) == -1 \
    || GPIOWrite(LED_3,1) == -1) {
        printf("GPIOExport Write\n");
    }
  /*function start--------------------------------------------------------------------------------------*/
    thr_id = pthread_create(&p_thread[0], NULL, heart, (void*)sock_ptr);
    if (thr_id < 0) {
        perror("thread create error : ");
        exit(0);
    }

    thr_id = pthread_create(&p_thread[1], NULL, joystick, (void*)sock_ptr);
    if (thr_id < 0) {
        perror("thread create error : ");
        exit(0);
    }
    
    pthread_join(p_thread[0],(void**)&status);
    pthread_join(p_thread[1],(void**)&status);
    is_exit = 0;
  }
}
