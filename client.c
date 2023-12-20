#include <arpa/inet.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <linux/i2c-dev.h>
#include <pthread.h>
#include <math.h>
#include "spi.h"
#include "gpio.h"

#define IN 0
#define OUT 1

#define LOW 0
#define HIGH 1

#define POUT 23
#define PIN 24

// ADXL345 I2C address
#define ADXL345_I2C_ADDR 0x53

// ADXL345 registers
#define POWER_CTL 0x2D
#define DATA_FORMAT 0x31
#define DATAX0 0x32

#define BUFFER_SIZE 5

// ADXL345 레지스터 주소
#define ADXL345_REG_DATA_X0 0x32
#define ADXL345_REG_DATA_X1 0x33
#define ADXL345_REG_DATA_Y0 0x34
#define ADXL345_REG_DATA_Y1 0x35
#define ADXL345_REG_DATA_Z0 0x36
#define ADXL345_REG_DATA_Z1 0x37

// I2C 디바이스 파일 디스크립터
int i2c_fd;
static const char *DEVICE = "/dev/spidev0.0";

char msg[2];
char buffer[BUFFER_SIZE];
int sock, fd;
int isstart = 0;
int file;
int cooltime[4] = {0, 0, 0, 0};
int outs[14] = {4, 17, 27, 5, 6, 12, 13, 19, 26, 16, 20, 21, 23, 24};

pthread_t p_thread[5];

short combineBytes(unsigned char msb, unsigned char lsb)
{
  return ((short)msb << 8) | lsb;
}

int setLed(int start, char color)
{
  start = start * 3;
  if (color == 'R')
  {
    if (GPIOWrite(outs[start], 1) == -1 || GPIOWrite(outs[start + 1], 0) == -1 || GPIOWrite(outs[start + 2], 0) == -1)
    {
      return 3;
    }
  }
  else if (color == 'G')
  {
    if (GPIOWrite(outs[start], 0) == -1 || GPIOWrite(outs[start + 1], 1) == -1 || GPIOWrite(outs[start + 2], 0) == -1)
    {
      return 3;
    }
  }
  else if (color == 'B')
  {
    if (GPIOWrite(outs[start], 0) == -1 || GPIOWrite(outs[start + 1], 0) == -1 || GPIOWrite(outs[start + 2], 1) == -1)
    {
      return 3;
    }
  }
  else if (color == 'Y')
  {
    if (GPIOWrite(outs[start], 1) == -1 || GPIOWrite(outs[start + 1], 0) == -1 || GPIOWrite(outs[start + 2], 1) == -1)
    {
      return 3;
    }
  }
  else
  {
    if (GPIOWrite(outs[start], 0) == -1 || GPIOWrite(outs[start + 1], 0) == -1 || GPIOWrite(outs[start + 2], 0) == -1)
    {
      return 3;
    }
  }
}

void error_handling(char *message)
{
  fputs(message, stderr);
  fputc('\n', stderr);
  exit(1);
}

void *sensorThread()
{

  int writesize;
  int pressureValue, soundValue;
  clock_t start_t, end_t;
  double rtime, distance;
  int sum = 0;
  int xAccl, yAccl, zAccl;
  int prevx = 0, prevy = 0, prevz = 0;

  while (isstart)
  {
    sum = 0;
    snprintf(buffer, sizeof(buffer), "%s", "0000");
    srand(time(NULL));  

    for (int i = 0; i < 4; i++)
    {
      if (cooltime[i] != 0)
      {
        setLed(i, 'Y');
      }
      else
      {
        setLed(i, 'K');
      }
    }

    // 압력
    pressureValue = readadc(fd, 5);
    printf("압력 value: %d\n", pressureValue);
    if (pressureValue > 800)
    {
      buffer[0] = '1';
      sum++;
    }
    else if (pressureValue > 500)
    {
      buffer[0] = '2';
    }
    else
    {
      buffer[0] = '0';
    }

    // 가속도
    char reg[1] = {DATAX0};
    write(file, reg, 1);
    char data[6] = {0};

    if (read(file, data, 6) != 6)
    {
      printf("Input/output error.\n");
      exit(1);
    }
    else
    {
      // Convert the data
      xAccl = (data[1] * 256 + data[0]);
      if (xAccl > 32767)
      {
        xAccl -= 65536;
      }

      yAccl = (data[3] * 256 + data[2]);
      if (yAccl > 32767)
      {
        yAccl -= 65536;
      }

      zAccl = (data[5] * 256 + data[4]);
      if (zAccl > 32767)
      {
        zAccl -= 65536;
      }

      // Output data to screen
      // printf("Acceleration in Axis: %d %d %d\n", xAccl, yAccl, zAccl);
      int sum1 = abs(prevx - xAccl) + abs(prevy - yAccl) + abs(prevz - zAccl);
      printf("가속도 합 : %d\n",sum);
      if (sum1 > 900)
      {
        buffer[1] = '1';
        sum++;
      }
      else if (sum1 > 600)
      {
        buffer[1] = '2';
      }
      else
      {
        buffer[1] = '0';
      }
    }
    prevx = xAccl;
    prevy = yAccl;
    prevz = zAccl;
    // 소음
    soundValue = readadc(fd, 6);
    printf("소음 value: %d\n", soundValue);
    if (soundValue > 700)
    {
      buffer[2] = '1';
      sum++;
    }
    else if (soundValue > 500)
    {
      buffer[2] = '2';
    }
    else
    {
      buffer[2] = '0';
    }


    // 초음파
    if (-1 == GPIOWrite(POUT, 1))
    {
      printf("gpio write/trigger err\n");
      return (3);
    }

    GPIOWrite(POUT, 0);

    while (GPIORead(PIN) == 0)
    {
      start_t = clock();
    }

    while (GPIORead(PIN) == 1)
    {
      end_t = clock();
    }

    rtime = (double)(end_t - start_t) / CLOCKS_PER_SEC;

    distance = rtime / 2 * 34000;
    printf("초음파 distance : %.2lfcm\n", distance);
    if (distance < 6)
    {
      buffer[3] = '1';
      sum++;
    }
    else if (distance < 15)
    {
      buffer[3] = '2';
    }
    else
    {
      buffer[3] = '0';
    }
    if (sum >= 3)
    {
      int index;
      if (sum == 4)
      {
        index = rand() % 4;
      }
      else
      {
        for (int i = 0; i < 4; i++)
        {
          if (buffer[i] != '1')
          {
            index = i;
            break;
          }
        }
      }

      buffer[index] = '0';

      for (int i = 0; i < 4; i++)
      {
        if (buffer[i] == '1')
        {
          setLed(i, 'R');
          cooltime[i] = (rand() % 4) + 4;
        }
        else
        {
          setLed(i, 'K');
        }
      }
    }
    else
    {
      for (int i = 0; i < 4; i++)
      {
        if (cooltime[i] != 0)
        {
          buffer[i] = '0';
          setLed(i, 'Y');
        }
        else
        {
          if (buffer[i] == '1')
          {
            setLed(i, 'R');
            cooltime[i] = (rand() % 4) + 4;
          }
          else if (buffer[i] == '2')
          {
            setLed(i, 'G');
            cooltime[i] = (rand() % 4) + 4;
          }
          else
          {
            setLed(i, 'B');
          }
        }
      }
    }

    writesize = write(sock, buffer, 5);
    if (writesize < 0)
    {
      error_handling("Error writing to socket");
    }
    printf("%s\n", buffer);
    
    // usleep(100000);
    // usleep(100000);

    sleep(1);
  }
}
void *gameControlThread()
{
  int readsize;
  while (1)
  {
    readsize = read(sock, msg, 2);
    if (readsize <= 0)
    {
      printf("[*] session closed\n");
      break;
    }
    printf("msg : %s\n", msg);
    if (!strcmp(msg, "1"))
    {
      isstart = 1;
      pthread_create(&p_thread[1], NULL, sensorThread, NULL);
    }
    else if (!strcmp(msg, "0"))
    {
      isstart = 0;
      pthread_cancel(p_thread[1]);
      pthread_join(p_thread[1], NULL);
      for (int i = 0; i < 12; i++)
      {
        if (GPIOWrite(outs[i], 0) == -1)
        {
          return 2;
        }
      }
    }
  }
}

void *timeCheckThread()
{

  while (1)
  {
    for (int i = 0; i < 4; i++)
    {
      if (cooltime[i] > 0)
      {
        cooltime[i]--;
      }
    }
    // usleep(100000);
    sleep(1);
  }
}

int main(int argc, char *argv[])
{

  struct sockaddr_in serv_addr;
  int thr_id;
  int status;
  snprintf(buffer, sizeof(buffer), "%s", "0000");
  if (argc != 3)
  {
    printf("Usage : %s <IP> <port>\n", argv[0]);
    exit(1);
  }

  // gpio init

  for (int i = 0; i < 14; i++)
  {
    if (GPIOExport(outs[i]) == -1)
    {
      return 1;
    }
  }

  for (int i = 0; i < 13; i++)
  {
    if (GPIODirection(outs[i], OUT) == -1)
    {
      return 2;
    }
  }
  if (GPIODirection(PIN, IN) == -1)
  {
    return 2;
  }

  // spi init
  fd = open(DEVICE, O_RDWR);
  if (fd <= 0)
  {
    perror("Device open error");
    return -1;
  }

  if (prepare(fd) == -1)
  {
    perror("Device prepare error");
    return -1;
  }

  // i2c init

  char *bus = "/dev/i2c-1";
  if ((file = open(bus, O_RDWR)) < 0)
  {
    printf("Failed to open the bus.\n");
    exit(1);
  }
  ioctl(file, I2C_SLAVE, ADXL345_I2C_ADDR);

  // ADXL345 initialization
  char config[2] = {0};
  config[0] = POWER_CTL;
  config[1] = 0x08;
  write(file, config, 2);
  config[0] = DATA_FORMAT;
  config[1] = 0x08;
  write(file, config, 2);

  // socket init
  sock = socket(PF_INET, SOCK_STREAM, 0);
  if (sock == -1)
    error_handling("socket() error");

  memset(&serv_addr, 0, sizeof(serv_addr));
  serv_addr.sin_family = AF_INET;
  serv_addr.sin_addr.s_addr = inet_addr(argv[1]);
  serv_addr.sin_port = htons(atoi(argv[2]));

  if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) == -1)
    error_handling("connect() error");

  pthread_create(&p_thread[0], NULL, timeCheckThread, NULL);
  gameControlThread();

  pthread_cancel(p_thread[0]);
  pthread_join(p_thread[0], NULL);

  close(i2c_fd);
  close(sock);

  // for (int i = 0; i < 14; i++)
  // {
  //   if (GPIOUnexport(outs[i]) == -1)
  //   {
  //     return 4;
  //   }
  // }
  return (0);
}
