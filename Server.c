#include <arpa/inet.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <pthread.h>
#include <time.h>
#include "gpio.h"
#include "pwm.h"


#define POUT 18
#define PIN1 20
#define POUT1 21

//--------------------------------------------------------------------
// 전역변수 정의
//--------------------------------------------------------------------
typedef struct Game_DATA {
    // 공격 => 서버 => 도트
    char grade[4];              // 공격 라인 및 등급
    // 수비 => 서버 => 도트
    char direction[1];          // 방향 0~4 
    // 수비 => 서버
    int life;               // 살아있는지(1,0)
    // 도트 => 서버 => 수비
    int attack;             // 적중 여부(1,0)         
    int time;               // 시간 종료 여부
}DATA;

DATA DATA_set;
int clnt_sock_DOT = -1 ;
int clnt_sock_ATTACK = -1;
int clnt_sock_DEFENSE = -1;
pthread_t p_thread[7];
int segments[8] =  {11,6,23,8,7,10,16,25};    
int digits[4] = {22,27,17,24};
int num[][7] = {
    {1, 1, 1, 1, 1, 1, 0}, // 0
    {0, 1, 1, 0, 0, 0, 0}, // 1
    {1, 1, 0, 1, 1, 0, 1}, // 2
    {1, 1, 1, 1, 0, 0, 1}, // 3
    {0, 1, 1, 0, 0, 1, 1}, // 4
    {1, 0, 1, 1, 0, 1, 1}, // 5
    {1, 0, 1, 1, 1, 1, 1}, // 6
    {1, 1, 1, 0, 0, 0, 0}, // 7
    {1, 1, 1, 1, 1, 1, 1}, // 8
    {1, 1, 1, 1, 0, 1, 1}  // 9
};
int Current_time = 180;
//--------------------------------------------------------------------
// Error 처리
//--------------------------------------------------------------------
void error_handling(char *message) {
  fputs(message, stderr);
  fputc('\n', stderr);
  exit(1);
}
//--------------------------------------------------------------------
// Thread Function
//--------------------------------------------------------------------


void *DOT_function() {
    
    //----------------------------------------------------------------
    // 받아올 msg 초기화
    //----------------------------------------------------------------
	char msg;
	memset(&msg, 0, sizeof(msg));
    
    //----------------------------------------------------------------
    // 시간이 종료되지 않았고, 수비 플레이어가 살아있는 경우에 무한 루프
    //----------------------------------------------------------------
	while (DATA_set.time == 1 && DATA_set.life == 1) {
        //------------------------------------------------------------
        // 전송할 데이터 포맷으로 변환 (다섯자리 수)
        //------------------------------------------------------------
        
        if(read(clnt_sock_DOT, &msg, sizeof(char)) == -1) printf("Dot read error");
        DATA_set.attack = msg - '0'; 

        if (DATA_set.attack == 1){
            printf("[수비 플레이어 적중] 수비 체력 -1\n");
            char Data_attack;
            Data_attack = DATA_set.attack + '0';
            //snprintf(&Data_attack,1,"%d",DATA_set.attack);
            
            write(clnt_sock_DEFENSE, &Data_attack, sizeof(char));
            DATA_set.attack = 0;
        } 
        usleep(100*1000);
	} 
    //----------------------------------------------------------------
    // 수비 캐릭터 사망 시 
    //----------------------------------------------------------------
    write(clnt_sock_DOT, "99999", sizeof(char) * 5);
    printf("dot 종료\n");
    //----------------------------------------------------------------
}
void *DOT_write_function(void *data) {
    
    //----------------------------------------------------------------
    // 시간이 종료되지 않았고, 수비 플레이어가 살아있는 경우에 무한 루프
    //----------------------------------------------------------------
	while (DATA_set.time == 1 && DATA_set.life == 1) {
        //------------------------------------------------------------
        // 전송할 데이터 포맷으로 변환 (다섯자리 수)
        //------------------------------------------------------------
        char msg_data[5];
        memset(msg_data, 0, sizeof(msg_data)); 
        strcpy(msg_data, DATA_set.direction); 
        strcat(msg_data, DATA_set.grade);   
        if (msg_data[4] == 0) msg_data[4] = '0';

		write(clnt_sock_DOT, msg_data, sizeof(msg_data)); 
        memset(DATA_set.grade, '0', sizeof(DATA_set.grade));
        memset(DATA_set.direction, '0', sizeof(DATA_set.direction));
        usleep(100*1000);
	} 
}
void *ATTACK_function() {
    //----------------------------------------------------------------
    // 받아올 msg 초기화
    //----------------------------------------------------------------
	char msg[5];
    int str_len;
	memset(msg, 0, sizeof(msg));    
    //----------------------------------------------------------------
    // 시간이 종료되지 않았고, 수비 플레이어가 살아있는 경우에 무한 루프
    //----------------------------------------------------------------
	while (DATA_set.time == 1 && DATA_set.life == 1) {
        if(str_len = read(clnt_sock_ATTACK, msg, sizeof(msg)) == -1)  printf("ATTACK READ ERROR\n");
        printf("msg: %s\n",msg);
        strcpy(DATA_set.grade, msg);
        DATA_set.grade[4] = 0;
        // for (int i = 0; i < 4; i++)
        //     DATA_set.grade[i] = msg[i];
	} 
    //----------------------------------------------------------------
    // 수비 캐릭터 사망 시 
    //----------------------------------------------------------------
    char send_data[] = "0";
    write(clnt_sock_ATTACK, send_data, sizeof(send_data));
    printf("attack 종료\n");
    //----------------------------------------------------------------
}
void *DEFENSE_function() {
    //----------------------------------------------------------------
    // 받아올 msg 초기화
    //----------------------------------------------------------------
	char msg[5];
    //----------------------------------------------------------------
    // 전송할 데이터 포맷으로 변환 (네자리 수)
    //----------------------------------------------------------------
    char send_data[] = "9999";
    //----------------------------------------------------------------
    // 시간이 종료되지 않았고, 수비 플레이어가 살아있는 경우에 무한 루프
    //----------------------------------------------------------------
	while (DATA_set.time == 1 && DATA_set.life == 1) {
        memset(msg, 0, sizeof(msg));
        if (read(clnt_sock_DEFENSE, msg, sizeof(msg)) == -1) printf("Defence read error");
        int input_data = atoi(msg);
        if(input_data == -1)    DATA_set.life = 0;
        else {
            if ('0' <= msg[0] && msg[0] <= '9')
                DATA_set.direction[0] = msg[0];
            else
                DATA_set.direction[0] = '0';
            //strcpy(DATA_set.direction,msg);
        }
	}
    //----------------------------------------------------------------
    // 수비 캐릭터 사망 시 
    //----------------------------------------------------------------
    if (DATA_set.life == 0) {
        printf("[GAME FINISH] 수비 캐릭터 사망\n");
        DATA_set.time = 0; //게임 초기화
        Current_time = 0;
        
        PWMWriteDutyCycle(0, 1000000);
        usleep(300000);  // Wait for 1 seconds
        PWMWriteDutyCycle(0, 0);
        usleep(100000);
        PWMWriteDutyCycle(0, 1000000);
        usleep(300000);  // Wait for 1 seconds
        PWMWriteDutyCycle(0, 0);
        for (int i = 0; i < 3; i++) {
            GPIOWrite(digits[i], 1);
        }
        for (int j = 0; j < 7; j++) {
            GPIOWrite(segments[j], 0);
        }
    }
    write(clnt_sock_DEFENSE, send_data, sizeof(send_data));
    //----------------------------------------------------------------
}
void *Time_function() {
    //--------------------------------------------------------------------
    // TODO : 부저는 여기에 60초씩 울리게 만들기
    //--------------------------------------------------------------------
    int Seg_time[4];
    while (Current_time > 0 && DATA_set.time > 0)
    {
        Seg_time[0] = Current_time / 60;
        Seg_time[1] = (Current_time % 60) / 10;
        Seg_time[2] = (Current_time % 60) % 10;
        for (int i = 0; i < 3; i++) {
            for (int j = 0; j < 7; j++) {
                GPIOWrite(segments[j], num[Seg_time[i]][j]);
                if (i == 0)
                {
                    GPIOWrite(25,1);
                }
                else 
                {
                    GPIOWrite(25,0);
                }
            }
            GPIOWrite(digits[i], 0);
            usleep(100);
            GPIOWrite(digits[i], 1);
        }
    }
    DATA_set.time = 0;
    printf("[GAME FINISH] 시간 종료\n");
    for (int i = 0; i < 3; i++) {
        GPIOWrite(digits[i], 1);
    }
    for (int j = 0; j < 7; j++) {
        GPIOWrite(segments[j], 0);
    }
    DATA_set.time = 0; //게임 초기화
    PWMWriteDutyCycle(0, 1000000);
    usleep(300000);  // Wait for 1 seconds
    PWMWriteDutyCycle(0, 0);
    usleep(100000);
    PWMWriteDutyCycle(0, 1000000);
    usleep(300000);  // Wait for 1 seconds
    PWMWriteDutyCycle(0, 0);
}
void *CountDown() {
    while (Current_time > 0)
    {
        sleep(1);
        printf("공격 4자리: %s\n", DATA_set.grade);
        printf("수비 방향 1자리: %s\n",DATA_set.direction);
        Current_time --;
    }
}
int main(int argc, char *argv[]) {

    // Initialization
    int state = 1;
    int prev_state = 1;
    int serv_sock;

    struct sockaddr_in serv_addr, clnt_addr;
    socklen_t clnt_addr_size;
    
    int thr_id;
    int status;
    char p1[] = "thread_DOT";       // 도트 매트릭스 RPI
    char p2[] = "thread_ATTACK";    // 공격 RPI
    char p3[] = "thread_DEFENSE";   // 방어 RPI
    //--------------------------------------------------------------------
    // port 열기
    //----------------------------------------------------------------
    if (argc != 2) {
        printf("Usage : %s <port>\n", argv[0]);
    }
    printf("loading ...\n");
    //--------------------------------------------------------------------
    // BUTTON GPIO setting
    //--------------------------------------------------------------------
    if (-1 == GPIOExport(PIN1) || -1 == GPIOExport(POUT1)) return (1);
    if (-1 == GPIODirection(PIN1, IN) || -1 == GPIODirection(POUT1, OUT))
        return (2);
    if (-1 == GPIOWrite(POUT1, 1)) return (3);
    //--------------------------------------------------------------------
    // 7-Segment GPIO setting
    //--------------------------------------------------------------------
    for (int i = 0; i < 8; i++) {
        if (-1 == GPIOExport(segments[i])) return (1);
        if (-1 == GPIODirection(segments[i], OUT))
            return (2);  
        usleep(2000*100);
    }
    for (int i = 0; i < 4; i++) {
        if (-1 == GPIOExport(digits[i])) return (1);
        if (-1 == GPIODirection(digits[i], OUT))
            return (2);
        usleep(2000*100);
    }
    printf("Complete!\n");
    printf("------------------\n");
    printf("Socket\n");
    printf("------------------\n");

    //--------------------------------------------------------------------
    // BUZZER pwm setting
    //--------------------------------------------------------------------
    PWMExport(0);       // Use "0" for PWM0
    PWMWritePeriod(0, 2000000);  // Set period to 2ms (500Hz)
    PWMWriteDutyCycle(0, 0);  // Set duty cycle to 1ms (50% duty cycle)
    PWMEnable(0);
    //--------------------------------------------------------------------
    // server socket 
    //--------------------------------------------------------------------
    serv_sock = socket(PF_INET, SOCK_STREAM, 0);
    if (serv_sock == -1) error_handling("socket() error");

    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    serv_addr.sin_port = htons(atoi(argv[1]));

    if (bind(serv_sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) == -1)
    error_handling("bind() error");

    if (listen(serv_sock, 5) == -1) error_handling("listen() error");
    //--------------------------------------------------------------------
    // 도트 매트릭스 client connect
    //--------------------------------------------------------------------
    if (clnt_sock_DOT < 0) {

        clnt_addr_size = sizeof(clnt_addr);
        clnt_sock_DOT = accept(serv_sock, (struct sockaddr *)&clnt_addr, &clnt_addr_size);

        if (clnt_sock_DOT == -1) error_handling("accept() error");              // 오류 처리
    }
    printf("[DOT Client] Connection established\n");
    //--------------------------------------------------------------------
    // 공격 client connect
    //--------------------------------------------------------------------
    if (clnt_sock_ATTACK < 0) {

        clnt_addr_size = sizeof(clnt_addr);
        clnt_sock_ATTACK = accept(serv_sock, (struct sockaddr *)&clnt_addr, &clnt_addr_size);

        if (clnt_sock_ATTACK == -1) error_handling("accept() error");              // 오류 처리
        
    }
    printf("[ATTACK Client] Connection established\n");
    //--------------------------------------------------------------------
    // 방어 client connect
    //--------------------------------------------------------------------
    if (clnt_sock_DEFENSE < 0) {

        clnt_addr_size = sizeof(clnt_addr);
        clnt_sock_DEFENSE = accept(serv_sock, (struct sockaddr *)&clnt_addr, &clnt_addr_size);

        if (clnt_sock_DEFENSE == -1) error_handling("accept() error");              // 오류 처리

    }
    printf("[DEFENSE Client] Connection established\n");
    //--------------------------------------------------------------------
    // 게임 시작 및 종료
    //--------------------------------------------------------------------
    printf("------------------\n");
    printf("PRESS START BUTTON\n");
    printf("------------------\n");
    while (1)
    {
        state = GPIORead(PIN1);

        if (prev_state == 0 && state == 1)
        {
            DATA_set.time = (DATA_set.time + 1) % 2;
            if (DATA_set.time == 1)
            {
                //--------------------------------------------------------------------
                // 버튼 입력으로 게임이 시작된 case
                //--------------------------------------------------------------------
                // Thread
                char send_data[] = "1";
                Current_time = 180;
                memset(DATA_set.grade, '0', sizeof(DATA_set.grade));
                memset(DATA_set.direction, '0', sizeof(DATA_set.direction));
                DATA_set.life = 1;
                DATA_set.time = 1;
                
                write(clnt_sock_DOT, send_data, sizeof(send_data));
                write(clnt_sock_ATTACK, send_data, sizeof(send_data));
                write(clnt_sock_DEFENSE, send_data, sizeof(send_data));
            
                PWMWriteDutyCycle(0, 1000000);
                usleep(300000);  // Wait for 1 seconds
                PWMWriteDutyCycle(0, 0);
                printf("[GAME START]\n");
                
                thr_id = pthread_create(&p_thread[0], NULL, DOT_function, NULL);    // dot_thread 생성
                if (thr_id < 0) {
                    perror("thread create error : ");  
                    exit(0);
                }
                thr_id = pthread_create(&p_thread[1], NULL, ATTACK_function, NULL);    // ATTACK_thread 생성
                if (thr_id < 0) {
                    perror("thread create error : ");
                    exit(0);
                }
                thr_id = pthread_create(&p_thread[2], NULL, DEFENSE_function, NULL);    // defense_thread 생성
                if (thr_id < 0) {
                    perror("thread create error : ");
                    exit(0);
                }
                thr_id = pthread_create(&p_thread[3], NULL, Time_function, NULL);    // 세븐 세그먼트 생성
                if (thr_id < 0) {
                    perror("thread create error : ");
                    exit(0);
                }
                thr_id = pthread_create(&p_thread[4], NULL, CountDown, NULL);    // 카운트다운 생성
                if (thr_id < 0) {
                    perror("thread create error : ");
                    exit(0);
                }
                thr_id = pthread_create(&p_thread[5], NULL, DOT_write_function, NULL);    // defense_thread 생성
                if (thr_id < 0) {
                    perror("thread create error : ");
                    exit(0);
                }
                pthread_detach(p_thread[3]);
                pthread_detach(p_thread[4]);
                pthread_detach(p_thread[0]);
                pthread_detach(p_thread[1]);
                pthread_detach(p_thread[2]);
                pthread_detach(p_thread[5]);
            }
            else 
            {
                //--------------------------------------------------------------------
                // 버튼 입력으로 게임이 강제 종료된 case
                //--------------------------------------------------------------------
                char send_data[] = "9999";
                printf("[GAME FINISH] 플레이어 강제 종료\n");
                DATA_set.time = 0;
                PWMWriteDutyCycle(0, 1000000);
                usleep(300000);  // Wait for 1 seconds
                PWMWriteDutyCycle(0, 0);
                for (int i = 0; i < 3; i++) {
                    GPIOWrite(digits[i], 1);
                }
                for (int j = 0; j < 7; j++) {
                    GPIOWrite(segments[j], 0);
                }
                pthread_cancel(p_thread[0]);
                pthread_cancel(p_thread[1]);
                pthread_cancel(p_thread[2]);
                pthread_cancel(p_thread[3]);
                pthread_cancel(p_thread[4]);
                pthread_cancel(p_thread[5]);
                write(clnt_sock_DOT, send_data, sizeof(send_data));
                write(clnt_sock_ATTACK, send_data, sizeof(send_data));
                write(clnt_sock_DEFENSE, send_data, sizeof(send_data));
            }
        }
        prev_state = state;
        usleep(500 * 100);
    }
    //--------------------------------------------------------------------
    close(clnt_sock_DOT);
    close(clnt_sock_ATTACK);
    close(clnt_sock_DEFENSE);
    close(serv_sock);

    return (0);
}
