#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<errno.h>
#include<sys/types.h>
#include<sys/socket.h>
#include<netinet/in.h>
#include <signal.h>
#include <sys/prctl.h>
#include <fcntl.h>
#include<time.h>
#include <sys/select.h>
#define MAXLINE 1024
#define MAXSIZE 1024

//阻塞与非阻塞模式
#define NON_BLOCK   0
#define IS_BLOCK    1

//一次申请socket的数目
#define FD_CNT_PER_TIME    50
#define STAT_CNT    11

#define TIMER_PIECE  3
#define RETRY_TIME   62
//状态
enum STAT{
    INIT,
    READ_STUNO,
    WRITE_STUNO,

    READ_PID,
    WRITE_PID,

    READ_TIME,
    WRITE_TIME,

    READ_STR,
    WRITE_STR,

    READ_END,
    WRITE_FILE,
    COMPLETED,//11

    ERR_CONNECT,
    ERR_READ,   //13
    ERR_WRITE
};

//收到数据的关键字
#define STUNO    "StuNo"
#define PID    "pid"
#define TIME    "TIME"
#define STR    "str"
#define END    "end"
//收发数据时对数据长度的要求
#define STUNO_READ_LEN    5
#define STUNO_WRITE_LEN    4
#define PID_READ_LEN    3
#define PID_WRITE_LEN    4
#define TIME_READ_LEN    5
#define TIME_WRITE_LEN    19
#define STR_READ_LEN    9
#define END_READ_LEN    3

typedef struct{
    int stuno;
    pid_t pid;
    char time_str[20];
    char *buf;
    int str_len;
}msg_cli;
