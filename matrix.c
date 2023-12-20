#include <pthread.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <signal.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <unistd.h>
#include <stdbool.h>
#include "gpio.h"
#include "lst.h"

const int CS[3] = {10, 0, 0};
#define DIN 12
#define CLK 18

#define CS_NUM 1

#define DECODE_MODE 0x09
#define DECODE_MODE_VAL 0x00

#define INTENSITY 0x0a
#define INTENSITY_VAL 0x01

#define SCAN_LIMIT 0x0b
#define SCAN_LIMIT_VAL 0x07
#define POWER_DOWN 0x0c
#define POWER_DOWN_VAL 0x01
#define TEST_DISPLAY 0x0f
#define TEST_DISPLAY_VAL 0x00

#define HEIGHT 8
#define WIDTH 24

#define FAST_BULLET 200
#define SLOW_BULLET 300

enum Directions
{
	UP = 1,
	DOWN,
	LEFT,
	RIGHT
};

typedef struct s_player
{
	short row, col;
	short health;
} Player;

typedef struct s_bullet
{
	int lane, col, grade;
	long long halt_time;
} Bullet;

Player player;

unsigned char matrix[3][8];
unsigned char board[HEIGHT][WIDTH];
short finished, state_to_finish;
int sock;
char msg[6] = {'0', '0', '0', '0', '0'};
t_list *bullets;
pthread_mutex_t string_mtx, board_mtx, matrix_mtx, bullet_mtx;

void ready_to_send(int cs, unsigned short address, unsigned short data);
void init_matrix();

int init_GPIO()
{
	for (int i = 0; i < CS_NUM; i++)
	{
		if (GPIOExport(CS[i]) == -1)
			return -1;
	}
	if (GPIOExport(DIN) == -1)
		return -1;
	if (GPIOExport(CLK) == -1)
		return -1;
	for (int i = 0; i < CS_NUM; i++)
	{
		if (GPIODirection(CS[i], OUT) == -1)
			return -1;
	}
	if (GPIODirection(DIN, OUT) == -1)
		return -1;
	if (GPIODirection(CLK, OUT) == -1)
		return -1;

	init_matrix();

	player.row = 0;
	player.col = 0;
	player.health = 3;

	return 1;
}

void init_matrix()
{
	for (int cs = 0; cs < CS_NUM; cs++)
	{
		ready_to_send(CS[cs], DECODE_MODE, DECODE_MODE_VAL);
		ready_to_send(CS[cs], INTENSITY, INTENSITY_VAL);
		ready_to_send(CS[cs], SCAN_LIMIT, SCAN_LIMIT_VAL);
		ready_to_send(CS[cs], POWER_DOWN, POWER_DOWN_VAL);
		ready_to_send(CS[cs], TEST_DISPLAY, TEST_DISPLAY_VAL);
	}
}

void init_mutex()
{
	pthread_mutex_init(&string_mtx, NULL);
	pthread_mutex_init(&board_mtx, NULL);
	pthread_mutex_init(&matrix_mtx, NULL);
	pthread_mutex_init(&bullet_mtx, NULL);
}

void send_16bits(unsigned short data)
{
	for (int i = 16; i > 0; i--)
	{
		unsigned short mask = 1 << (i - 1);
		GPIOWrite(CLK, 0);
		GPIOWrite(DIN, (data & mask) != 0);
		GPIOWrite(CLK, 1);
	}
}

void send_MAX7219(unsigned short reg_number, unsigned short data)
{
	send_16bits((reg_number << 8) + data);
}

void ready_to_send(int cs, unsigned short address, unsigned short data)
{
	GPIOWrite(cs, LOW);
	send_MAX7219(address, data);
	send_MAX7219(address, data);
	send_MAX7219(address, data);
	GPIOWrite(cs, HIGH);
}

void draw_dot(int cs, int row, int col)
{
	matrix[cs][row] |= (1 << col);
}

void update_matrix()
{
	/*      for (int i = 0 ; i < 3; i++) {
					for (int j=0;j<8;j++) {
							printf("%x ", matrix[i][j]);
					} printf("\n");
			} printf("\n");
	*/

	for (int i = 0; i < 8; i++)
	{
		GPIOWrite(CS[0], LOW);
		for (int j = 2; j >= 0; j--)
			send_MAX7219(i + 1, matrix[j][i]);
		GPIOWrite(CS[0], HIGH);
	}
}

void board_to_matrix()
{

	/*      for (int i = 0; i < CS_NUM; i++) {
					a = 0;
					for (int j = 0; j < 8; j++) {
							for (int k = 0; k < 8; k++) {
									if (board[j][i * 8 + k])
											a |= 1;
									a <<= 1;
							}
							a >>= 1;
							matrix[i][j] = a;
					}
			}
	*/
	memset(matrix, 0, sizeof(matrix));
	for (int i = 0; i < 8; i++)
	{
		for (int j = 0; j < 24; j++)
		{
			if (board[i][j] == 0)
				continue;
			int a, my, mx = i;
			if (j < 8)
				a = 1;
			else if (j < 16)
				a = 2;
			else
				a = 3;
			my = a * 8 - 1 - j;
			matrix[a - 1][my] |= (1 << (7 - mx));
		}
	}
}

void update_board(t_list *bullets)
{
	t_list *prev = bullets;
	t_list *cur = bullets->next;
	bool flag = false;

	memset(board, 0, sizeof(board));

	for (int i = 0; i < 2; i++)
	{
		for (int j = 0; j < 2; j++)
		{
			board[player.row + i][player.col + j] |= 1;
		}
	}

	while (cur)
	{
		Bullet *cur_bullet = (Bullet *)cur->bullet;
		int lane = cur_bullet->lane * 2;
		for (int i = 0; i < 2; i++)
		{
			board[lane + i][cur_bullet->col] |= 2;
			if (board[lane + i][cur_bullet->col] == 3)
			{
				flag = true;
				prev->next = cur->next;
				lstdelone(cur);
				cur = prev;
				break;
			}
		}
		prev = cur;
		cur = cur->next;
	}

	pthread_mutex_lock(&matrix_mtx);
	board_to_matrix();
	update_matrix();
	pthread_mutex_unlock(&matrix_mtx);
	if (flag)
	{
		printf("Hit!\n");
		// socket
		write(sock, "1", 1);
	}
}

int move_player(short key)
{
	if (key == UP)
	{
		if (player.row <= 0)
			return 0;
		--player.row;
	}

	else if (key == DOWN)
	{
		if (player.row >= 6)
			return 0;
		++player.row;
	}

	else if (key == LEFT)
	{
		if (player.col <= 0)
			return 0;
		--player.col;
	}

	else if (key == RIGHT)
	{
		if (player.col >= 6)
			return 0;
		++player.col;
	}
	else
		return 0;
	printf("row, col: %d %d\n", player.row, player.col);
	pthread_mutex_lock(&board_mtx);
	update_board(bullets);
	pthread_mutex_unlock(&board_mtx);
	return 1;
}

void test_led()
{
	usleep(1000 * 500);
	int a = -1;
	while (1)
	{
		memset(matrix, 0, sizeof(matrix));
		a = (a + 1) & 0x07;
		if (a & 1)
		{
			for (int i = 0; i < 4; i++)
			{
				draw_dot(0, a, i);
				//                              draw_dot(1, a, i);
				//                              draw_dot(2, a, i);
			}
			for (int i = 5; i <= 7; i++)
				draw_dot(1, a, i);
		}
		else
		{
			for (int i = 4; i < 8; i++)
			{
				draw_dot(0, a, i);
				//                              draw_dot(1, a, i);
				//                              draw_dot(2, a, i);
			}
			for (int i = 1; i <= 3; i++)
				draw_dot(1, a, i);
		}
		for (int i = 1; i < 3; i++)
			draw_dot(2, a, i + 2);

		update_matrix();
		printf("a: %d\n", a);

		usleep(1000 * 200);
	}
}

void error_handling(char *str)
{
	printf("%s\n", str);
	exit(1);
}

long long get_millisec(char is_first)
{
	static struct timeval start;
	struct timeval cur;

	if (!is_first)
	{
		gettimeofday(&cur, NULL);
		return (cur.tv_sec - start.tv_sec) * 1000 +
			   (cur.tv_usec - start.tv_usec) / 1000;
	}
	return (gettimeofday(&start, NULL));
}

void *input_from_server(void *arg)
{
	char tmp[6];

	while (1)
	{
		// read from socket
		usleep(1);
		int len = read(sock, tmp, sizeof(tmp));
		if (len == -1)
		{
            state_to_finish = 1;
			finished = 1;
			printf("socket disconnected\n");
			return NULL;
		}
		printf("tmp: %s\n", tmp);
		if (tmp[0] == '9' || len == 0)
		{
			finished = 1;
			printf("Finished!!\n");
			return NULL;
		}
		if (len <= 4 || tmp[0] < '0')
			continue;
		printf("len: %d\n", len);
		pthread_mutex_lock(&string_mtx);
		strcpy(msg, tmp);
		pthread_mutex_unlock(&string_mtx);
		// exit status -> return NULL
	}
	return NULL;
}

void *move_player_from_input(void *arg)
{
	char ch;

	update_matrix();
	while (1)
	{
		usleep(10);
		if (finished)
			return NULL;
		pthread_mutex_lock(&string_mtx);
		ch = msg[0];
		msg[0] = '0';
		pthread_mutex_unlock(&string_mtx);
		if (ch == '0')
			continue;
		ch -= '0';
		printf("ch: %d\n", ch);
		move_player((short)ch);
	}

	return NULL;
}

Bullet *make_new_bullet(int lane, int grade)
{
	Bullet *new_bullet = malloc(sizeof(Bullet));

	new_bullet->lane = lane;
	new_bullet->grade = grade;
	new_bullet->col = WIDTH - 1;
	new_bullet->halt_time = get_millisec(0);

	return new_bullet;
}

void *make_bullet(void *arg)
{
	char bul[5];

	while (1)
	{
		if (finished)
			return NULL;
		pthread_mutex_lock(&string_mtx);
		strcpy(bul, msg + 1);
		for (int i = 1; i <= 4; i++)
			msg[i] = '0';
		pthread_mutex_unlock(&string_mtx);

		int num = 0;
		Bullet *new_bullets[4];
		t_list *lst = NULL;
		t_list *cur;

		for (int i = 0; i < 4; i++)
		{
			if (bul[i] == '0')
				continue;
			new_bullets[num++] = make_new_bullet(i, bul[i] - '0');
		}

		if (num == 0)
			continue;

		lst = lstnew(new_bullets[0]);
		cur = lst;
		for (int i = 1; i < num; i++)
		{
			cur->next = lstnew(new_bullets[i]);
			cur = cur->next;
		}

		pthread_mutex_lock(&bullet_mtx);
		lstadd_back(&bullets, lst);
		pthread_mutex_unlock(&bullet_mtx);

		pthread_mutex_lock(&board_mtx);
		update_board(lst);
		pthread_mutex_unlock(&board_mtx);
	}

	return NULL;
}

void *move_bullet(void *arg)
{
	while (1)
	{
		short flag = 0;
		long long cur_millisec = get_millisec(0);

		if (finished)
			return NULL;
		pthread_mutex_lock(&bullet_mtx);

		t_list *prev = bullets;
		t_list *cur = bullets->next;
		while (cur)
		{
			Bullet *cur_bullet = (Bullet *)cur->bullet;
			int cool_time;
			if (cur_bullet->grade == 1)
				cool_time = FAST_BULLET;
			else if (cur_bullet->grade == 2)
				cool_time = SLOW_BULLET;

			if (cur_millisec - cur_bullet->halt_time > cool_time)
			{
				flag = 1;
				cur_bullet->halt_time = cur_millisec;
				if (--cur_bullet->col == -1)
				{
					prev->next = cur->next;
					lstdelone(cur);
					cur = prev;
				}
			}
			prev = cur;
			cur = cur->next;
		}
		pthread_mutex_unlock(&bullet_mtx);

		if (!flag)
			continue;
		pthread_mutex_lock(&board_mtx);
		update_board(bullets);
		pthread_mutex_unlock(&board_mtx);
	}

	return NULL;
}

pthread_t input_from_server_t, move_player_from_input_t, make_bullet_t, move_bullet_t;

int func()
{
	state_to_finish = 1;
	init_mutex();

	while (1)
	{
		memset(matrix, 0, sizeof(matrix));
		memset(board, 0, sizeof(board));
		matrix[0][6] = 0xC0;
		matrix[0][7] = 0xC0;
		pthread_create(&input_from_server_t, NULL, input_from_server, &sock);
		pthread_create(&move_player_from_input_t, NULL, move_player_from_inpu t, NULL);
		pthread_create(&make_bullet_t, NULL, make_bullet, NULL);
		pthread_create(&move_bullet_t, NULL, move_bullet, NULL);

		while (1)
		{
			//                      write(sock, "0", 1);
			//                      usleep(150 * 1000);
			usleep(100);
			if (finished)
			{
				sleep(1);
				break;
			}
		}

		lstclear(&bullets->next);
		pthread_join(input_from_server_t, NULL);
		pthread_join(move_player_from_input_t, NULL);
		pthread_join(make_bullet_t, NULL);
		pthread_join(move_bullet_t, NULL);
		memset(matrix, 0, sizeof(matrix));
		update_matrix();

		if (state_to_finish)
			break;
	}

	pthread_mutex_destroy(&string_mtx);
	pthread_mutex_destroy(&board_mtx);
	pthread_mutex_destroy(&matrix_mtx);
	pthread_mutex_destroy(&bullet_mtx);

	return 1;
}

void signal_handler(int signal)
{
	finished = 1;
	memset(matrix, 0, sizeof(matrix));
	update_matrix();
	exit(1);
}

void set_signal()
{
	struct sigaction act_int;
	struct sigaction act_quit;

	memset(&act_int, 0, sizeof(act_int));
	memset(&act_quit, 0, sizeof(act_quit));

	act_int.sa_flags = SA_RESTART;
	act_int.sa_handler = signal_handler;

	act_quit.sa_flags = SA_RESTART;
	act_quit.sa_handler = signal_handler;

	if (sigaction(SIGINT, &act_int, NULL) | sigaction(SIGQUIT, &act_quit, NULL))
		error_handling("sigaction error");
}

int main(int argc, char **argv)
{
	struct sockaddr_in serv_addr;

	if (init_GPIO() == -1)
	{
		printf("init failed\n");
		return 1;
	}

	if (argc != 3)
	{
		printf("Usage : %s <IP> <port>\n", argv[0]);
		return 1;
	}

	set_signal();
	memset(matrix, 0, sizeof(matrix));
	update_matrix();

	if (argc == 3)
	{
		char start_msg[2];
		start_msg[0] = '0';
		sock = socket(PF_INET, SOCK_STREAM, 0);
		if (sock == -1)
			error_handling("socket() error");

		memset(&serv_addr, 0, sizeof(serv_addr));
		serv_addr.sin_family = AF_INET;
		serv_addr.sin_addr.s_addr = inet_addr(argv[1]);
		serv_addr.sin_port = htons(atoi(argv[2]));

		if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) =
				= -1)
			error_handling("connect() error");

		printf("Connection established\n");
		while (start_msg[0] == '0')
		{
			read(sock, start_msg, sizeof(start_msg));
			if (start_msg[0] == '1')
			{
				printf("Game Start!\n");
				break;
			}
		}
	}

	memset(matrix, 0, sizeof(matrix));
	update_matrix();
	bullets = lstnew(NULL);
	get_millisec(1);

	if (argc == 2)
	{
		printf("Hiasdf\n");
		memset(matrix, 0, sizeof(matrix));
		update_matrix();
		test_led();
	}

	else
		func();
	return (0);
}
