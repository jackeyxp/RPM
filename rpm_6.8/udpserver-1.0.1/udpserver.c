
#include "app.h"
#include "bmem.h"

#include <sys/stat.h>

// STL must use g++...
// g++ -g udpserver.c bmem.c thread.cpp app.cpp tcpcamera.cpp tcproom.cpp tcpcenter.cpp tcpclient.cpp tcpthread.cpp room.cpp network.cpp student.cpp teacher.cpp -o udpserver -lrt -lpthread -ljson
// valgrind --tool=memcheck --leak-check=full --show-reachable=yes ./udpserver

CApp theApp;

#define LOG_MAX_SIZE                2048            // 单条日志最大长度(字节)...
#define LOG_MAX_FILE    40 * 1024 * 1024            // 日志文件最大长度(字节)...

char g_absolute_path[256] = {0};
char g_log_file_path[256] = {0};
void doSliceLogFile(struct tm * lpCurTm);
bool doGetCurFullPath(char * lpOutPath, int inSize);

int main(int argc, char **argv)
{
  // 获取进程当前运行完整路径...
  if( !doGetCurFullPath(g_absolute_path, 256) )
    return -1;
  // 构造日志文件完整路径...
  sprintf(g_log_file_path, "%s%s", g_absolute_path, "udpserver.log");
  // 增大文件打开数量...
  if( !theApp.doInitRLimit() )
    return -1;
  
  // 注意：阿里云专有网络无法获取外网地址，中心服务器可以同链接获取外网地址...
  // 因此，这个接口作废了，不会被调用，而是让中心服务器通过链接地址自动获取...
  //if( !theApp.doInitWanAddr() )
  //  return -1;
  
  // 创建udp服务器套接字...
  if( theApp.doCreateUdpSocket() < 0 )
    return -1;
  // 启动超时检测线程对象...
  if( !theApp.doStartThread() )
    return -1;
  // 阻塞循环等待网络数据到达...
  theApp.doWaitSocket();
  // 阻塞终止，退出进程...
  return 0;
}

// 返回全局的App对象...
CApp * GetApp() { return &theApp; }

// 获取当前进程全路径...
bool doGetCurFullPath(char * lpOutPath, int inSize)
{
  // 如果路径已经有效，直接返回...
  if( strlen(lpOutPath) > 0 )
    return true;
  // find the current path...
  int cnt = readlink("/proc/self/exe", lpOutPath, inSize);
  if( cnt < 0 || cnt >= inSize ) {
    fprintf(stderr, "readlink error: %s", strerror(errno));
    return false;
  }
  for( int i = cnt; i >= 0; --i ) {
    if( lpOutPath[i] == '/' ) {
      lpOutPath[i+1] = '\0';
      break;
    }
  }
  // append the slash flag...
  if( lpOutPath[strlen(lpOutPath) - 1] != '/' ) {  
    lpOutPath[strlen(lpOutPath)] = '/';  
  }
  return true;
}

// 对日志进行分片操作...
void doSliceLogFile(struct tm * lpCurTm)
{
  // 如果日志路径、日志文件路径、输入时间戳无效，直接返回...
  if( lpCurTm == NULL || strlen(g_absolute_path) <= 0 || strlen(g_log_file_path) <= 0 )
    return;
  // 获取日志文件的总长度，判断是否需要新建日志...
  struct stat dtLog = {0};
  if( stat(g_log_file_path, &dtLog) < 0 )
    return;
  // 日志文件不超过40M字节，直接返回...
  if( dtLog.st_size <= LOG_MAX_FILE )
    return;
  // 构造分片文件全路径名称...
  char szNewSliceName[256] = {0};
  sprintf(szNewSliceName, "%s%s_%d-%02d-%02d_%02d-%02d-%02d.log", g_absolute_path, "udpserver",
          1900 + lpCurTm->tm_year, 1 + lpCurTm->tm_mon, lpCurTm->tm_mday,
          lpCurTm->tm_hour, lpCurTm->tm_min, lpCurTm->tm_sec);
  // 对日志文件进行重新命名操作...
  rename(g_log_file_path, szNewSliceName);
}

// 全新的日志处理函数...
bool do_trace(const char * inFile, int inLine, bool bIsDebug, const char *msg, ...)
{
  // 获取日志文件全路径是否有效...
  if( strlen(g_log_file_path) <= 0 ) {
    fprintf(stderr, "log_file_path failed\n");
    return false;
  }
  // 准备日志头需要的时间信息...
  timeval tv;
  if(gettimeofday(&tv, NULL) == -1) {
    return false;
  }
  struct tm* tm;
  if((tm = localtime(&tv.tv_sec)) == NULL) {
    return false;
  }
  // 对日志进行分片操作...
  doSliceLogFile(tm);
  // 准备完整的日志头信息...
  int  log_size = -1;
  char log_data[LOG_MAX_SIZE] = {0};
  log_size = snprintf(log_data, LOG_MAX_SIZE, 
                "[%d-%02d-%02d %02d:%02d:%02d.%03d][%d][%s:%d][%s] ", 1900 + tm->tm_year, 1 + tm->tm_mon, tm->tm_mday,
                tm->tm_hour, tm->tm_min, tm->tm_sec, (int)(tv.tv_usec/1000), getpid(), inFile, inLine,
                bIsDebug ? "debug" : "trace");
  // 确认日志头长度...
  if(log_size == -1) {
    return false;
  }
  // 打开日志文件...
  int file_log_fd = -1;
  if( !bIsDebug ) {
    file_log_fd = open(g_log_file_path, O_RDWR|O_CREAT|O_APPEND, 0666);
    if( file_log_fd < 0 ) {
      return false;
    }
  }
  // 对数据进行格式化...
  va_list vl_list;
  va_start(vl_list, msg);
  log_size += vsnprintf(log_data + log_size, LOG_MAX_SIZE - log_size, msg, vl_list);
  va_end(vl_list);
  // 加入结尾符号...
  log_data[log_size++] = '\n';
  // 将格式化之后的数据写入文件...
  if( !bIsDebug ) {
    write(file_log_fd, log_data, log_size);
    close(file_log_fd);
  }
  // 将数据打印到控制台...
  fprintf(stderr, log_data);
  return true;
}

uint64_t os_gettime_ns(void)
{
	struct timespec ts;
	clock_gettime(CLOCK_MONOTONIC, &ts);
	return ((uint64_t) ts.tv_sec * 1000000000ULL + (uint64_t) ts.tv_nsec);
}

bool os_sleepto_ns(uint64_t time_target)
{
	uint64_t current = os_gettime_ns();
	if (time_target < current)
		return false;

	time_target -= current;

	struct timespec req, remain;
	memset(&req, 0, sizeof(req));
	memset(&remain, 0, sizeof(remain));
	req.tv_sec = time_target/1000000000;
	req.tv_nsec = time_target%1000000000;

	while (nanosleep(&req, &remain)) {
		req = remain;
		memset(&remain, 0, sizeof(remain));
	}

	return true;
}

void os_sleep_ms(uint32_t duration)
{
	usleep(duration*1000);
}

static inline void add_ms_to_ts(struct timespec *ts, unsigned long milliseconds)
{
	ts->tv_sec += milliseconds/1000;
	ts->tv_nsec += (milliseconds%1000)*1000000;
	if (ts->tv_nsec > 1000000000) {
		ts->tv_sec += 1;
		ts->tv_nsec -= 1000000000;
	}
}

int os_sem_init(os_sem_t **sem, int value)
{
	sem_t new_sem;
	int ret = sem_init(&new_sem, 0, value);
	if (ret != 0)
		return ret;

	*sem = (os_sem_t*)bzalloc(sizeof(struct os_sem_data));
	(*sem)->sem = new_sem;
	return 0;
}

void os_sem_destroy(os_sem_t *sem)
{
	if (sem) {
		sem_destroy(&sem->sem);
		bfree(sem);
	}
}

int os_sem_post(os_sem_t *sem)
{
	if (!sem) return -1;
	return sem_post(&sem->sem);
}

int os_sem_wait(os_sem_t *sem)
{
	if (!sem) return -1;
	return sem_wait(&sem->sem);
}

int os_sem_timedwait(os_sem_t *sem, unsigned long milliseconds)
{
  if (!sem) return -1;
  struct timespec ts;
  clock_gettime(CLOCK_REALTIME, &ts);
  add_ms_to_ts(&ts, milliseconds);
  return sem_timedwait(&sem->sem, &ts);
}

// 获取交互终端类型...
const char * get_tm_tag(int tmTag)
{
  switch(tmTag)
  {
    case TM_TAG_STUDENT: return "Student";
    case TM_TAG_TEACHER: return "Teacher";
    case TM_TAG_SERVER:  return "Server";
  }
  return "unknown";
}

// 获取交互终端身份...
const char * get_id_tag(int idTag)
{
  switch(idTag)
  {
    case ID_TAG_PUSHER: return "Pusher";
    case ID_TAG_LOOKER: return "Looker";
    case ID_TAG_SERVER: return "Server";
  }
  return "unknown";
}

// 获取用户类型...
const char * get_client_type(int inType)
{
  switch(inType)
  {
    case kClientPHP:       return "PHP";
    case kClientStudent:   return "Student";
    case kClientTeacher:   return "Teacher";
    case kClientUdpServer: return "UdpServer";
  }
  return "unknown";
}

// 获取命令类型...
const char * get_command_name(int inCmd)
{
  switch(inCmd)
  {
    case kCmd_Student_Login:        return "Student_Login";
    case kCmd_Student_OnLine:       return "Student_OnLine";
    case kCmd_Teacher_Login:        return "Teacher_Login";
    case kCmd_Teacher_OnLine:       return "Teacher_OnLine";
    case kCmd_UDP_Logout:           return "UDP_Logout";
    case kCmd_Camera_PullStart:     return "Camera_PullStart";
    case kCmd_Camera_PullStop:      return "Camera_PullStop";
    case kCmd_Camera_OnLineList:    return "Camera_OnLineList";
    case kCmd_Camera_LiveStart:     return "Camera_LiveStart";
    case kCmd_Camera_LiveStop:      return "Camera_LiveStop";
    case kCmd_UdpServer_Login:      return "UdpServer_Login";
    case kCmd_UdpServer_OnLine:     return "UdpServer_OnLine";
    case kCmd_UdpServer_AddTeacher: return "UdpServer_AddTeacher";
    case kCmd_UdpServer_DelTeacher: return "UdpServer_DelTeacher";
    case kCmd_UdpServer_AddStudent: return "UdpServer_AddStudent";
    case kCmd_UdpServer_DelStudent: return "UdpServer_DelStudent";
    case kCmd_PHP_GetUdpServer:     return "PHP_GetUdpServer";
    case kCmd_PHP_GetAllServer:     return "PHP_GetAllServer";
    case kCmd_PHP_GetAllClient:     return "PHP_GetAllClient";
    case kCmd_PHP_GetRoomList:      return "PHP_GetRoomList";
    case kCmd_PHP_GetPlayerList:    return "PHP_GetPlayerList";
  }
  return "unknown";
}
