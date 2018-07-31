
#pragma once

#include <unistd.h>
#include <sys/socket.h>     /* basic socket definitions */
#include <netinet/tcp.h>    /* 2017.07.26 - by jackey */
#include <netinet/in.h>     /* sockaddr_in{} and other Internet defns */
#include <arpa/inet.h>      /* inet(3) functions */
#include <sys/epoll.h>      /* epoll function */
#include <sys/types.h>      /* basic system data types */
#include <sys/resource.h>   /* setrlimit */
#include <sys/time.h>
#include <semaphore.h>
#include <fcntl.h>          /* nonblocking */
#include <stdarg.h>
#include <assert.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <stdio.h>
#include <algorithm> 
#include <string>
#include <list>
#include <map>

#include "rtp.h"

using namespace std;

#ifndef uint8_t
typedef unsigned char uint8_t;
#endif

#ifndef uint16_t
typedef unsigned short uint16_t;
#endif

#ifndef uint32_t
typedef unsigned int uint32_t;
#endif

#ifndef os_sem_t
struct os_sem_data {
	sem_t sem;
};
typedef struct os_sem_data os_sem_t;
#endif

class CApp;
class CRoom;
class CNetwork;
class CStudent;
class CTeacher;

typedef list<CTeacher *>        GM_ListTeacher; // 有补包的老师推流者列表...
typedef list<CStudent *>        GM_ListStudent; // 有丢包的学生观看者列表...
typedef map<int, CRoom *>       GM_MapRoom;     // RoomID => CRoom *
typedef map<int, CNetwork *>    GM_MapNetwork;  // PortID => CNetwork * => CStudent | CTeacher
typedef map<int, CStudent *>    GM_MapStudent;  // PortID => CStudent * => 学生端观看者列表
typedef map<uint32_t, rtp_lose_t>  GM_MapLose;  // 定义检测到的丢包队列 => 序列号 : 丢包结构体...

// 定义日志处理函数和宏 => debug 模式只打印不写日志文件...
bool do_trace(const char * inFile, int inLine, bool bIsDebug, const char *msg, ...);
#define log_trace(msg, ...) do_trace(__FILE__, __LINE__, false, msg, ##__VA_ARGS__)
#define log_debug(msg, ...) do_trace(__FILE__, __LINE__, true, msg, ##__VA_ARGS__)

// 获取全局的App对象...
CApp * GetApp();

uint64_t  os_gettime_ns(void);
void      os_sleep_ms(uint32_t duration);
bool      os_sleepto_ns(uint64_t time_target);

int       os_sem_init(os_sem_t **sem, int value);
void      os_sem_destroy(os_sem_t *sem);
int       os_sem_post(os_sem_t *sem);
int       os_sem_wait(os_sem_t *sem);
int       os_sem_timedwait(os_sem_t *sem, unsigned long milliseconds);

const char * get_client_type(int inType);
const char * get_command_name(int inCmd);

//////////////////////////////////////////////////////////////////////////
// 以下是有关TCP中转服务器的相关变量和类型定义...
//////////////////////////////////////////////////////////////////////////
class CTCPRoom;
class CTCPClient;
typedef map<int, CTCPRoom*>     GM_MapTCPRoom;  // RoomID   => CTCPRoom*
typedef map<int, CTCPClient*>   GM_MapTCPConn;  // connfd   => CTCPClient*
typedef map<string, string>     GM_MapJson;     // key      => value => JSON map object...

// define client type...
enum {
  kClientPHP     = 1,       // 网站端链接...
  kClientStudent = 2,       // 学生端链接...
  kClientTeacher = 3,       // 讲师端链接...
};

// define command id...
enum {
	kCmd_Student_Login      = 1,
  kCmd_Student_OnLine	    = 2,
  kCmd_Teacher_Login      = 3,
  kCmd_Teacher_OnLine     = 4,
};

// define the command header...
typedef struct {
  int   m_pkg_len;    // body size...
  int   m_type;       // client type...
  int   m_cmd;        // command id...
  int   m_sock;       // php sock in transmit...
} Cmd_Header;

///////////////////////////////////////////////////////////
// Only for transmit server...
//////////////////////////////////////////////////////////
typedef struct {
	char  pkg_len[8];  // body length, not including header
	char  cmd;         // command code
	char  status;      // status code for response
} TrackerHeader;
