
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
#include "common.h"

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

typedef list<CNetwork *>        GM_ListPusher;  // �в������������б� => ѧ��������|��ʦ������...
typedef list<CNetwork *>        GM_ListLooker;  // �ж����Ĺۿ����б� => ѧ���ۿ���|��ʦ�ۿ���...
typedef map<int, CRoom *>       GM_MapRoom;     // RoomID => CRoom *
typedef map<int, CNetwork *>    GM_MapNetwork;  // PortID => CNetwork * => CStudent | CTeacher
typedef map<int, CStudent *>    GM_MapStudent;  // PortID => CStudent * => ѧ���˹ۿ����б�
typedef map<uint32_t, rtp_lose_t>  GM_MapLose;  // �����⵽�Ķ������� => ���к� : �����ṹ��...

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
const char * get_tm_tag(int tmTag);
const char * get_id_tag(int idTag);
const char * get_abs_path();

int64_t buff2long(const char *buff);
void long2buff(int64_t n, char *buff);

//////////////////////////////////////////////////////////////////////////
// �������й�TCP��ת����������ر��������Ͷ���...
//////////////////////////////////////////////////////////////////////////
class CTCPRoom;
class CTCPClient;
class CTCPCamera;
typedef map<int, CTCPCamera*>   GM_MapTCPCamera;  // DBCameraID => CTCPCamera*
typedef map<int, CTCPRoom*>     GM_MapTCPRoom;    // RoomID     => CTCPRoom*
typedef map<int, CTCPClient*>   GM_MapTCPConn;    // connfd     => CTCPClient*
typedef map<string, string>     GM_MapJson;       // key        => value => JSON map object...
