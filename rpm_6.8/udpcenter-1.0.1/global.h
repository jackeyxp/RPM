
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

#include "../udpserver/common.h"

#define DEF_CENTER_PORT             26026          // Ĭ��UDP���ķ����������˿�...
#define MAX_OPEN_FILE                2048          // �����ļ������(��)...
#define MAX_EPOLL_SIZE               1024          // EPOLL�������ֵ...
#define MAX_LISTEN_SIZE              1024          // �����������ֵ...
#define CHECK_TIME_OUT                 10          // ��ʱ������� => ÿ��10�룬���һ�γ�ʱ...
#define APP_SLEEP_MS                  500          // Ӧ�ò���Ϣʱ��(����)...

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
class CTCPRoom;
class CUdpServer;
typedef map<int, CTCPRoom *>      GM_MapRoom;       // RoomID => CTCPRoom *
typedef map<int, CUdpServer *>    GM_MapServer;     // socket => CUdpServer *

// ������־�������ͺ� => debug ģʽֻ��ӡ��д��־�ļ�...
bool do_trace(const char * inFile, int inLine, bool bIsDebug, const char *msg, ...);
#define log_trace(msg, ...) do_trace(__FILE__, __LINE__, false, msg, ##__VA_ARGS__)
#define log_debug(msg, ...) do_trace(__FILE__, __LINE__, true, msg, ##__VA_ARGS__)

// ��ȡȫ�ֵ�App����...
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

int64_t buff2long(const char *buff);
void long2buff(int64_t n, char *buff);

//////////////////////////////////////////////////////////////////////////
// �������й�TCP��ת����������ر��������Ͷ���...
//////////////////////////////////////////////////////////////////////////
class CTCPClient;
typedef map<int, CTCPClient*>   GM_MapTCPConn;    // connfd     => CTCPClient*
typedef map<string, string>     GM_MapJson;       // key        => value => JSON map object...
