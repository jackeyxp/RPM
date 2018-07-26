
#include "app.h"
#include "bmem.h"

// STL must use g++...
// g++ -g udpserver.c bmem.c thread.cpp app.cpp tcpthread.cpp room.cpp network.cpp student.cpp teacher.cpp -o udpserver -lrt -lpthread -ljson
// valgrind --tool=memcheck --leak-check=full --show-reachable=yes ./udpserver

CApp theApp;

char g_absolute_path[256] = {0};
bool doGetLogPath(char * lpOutPath, int inSize);

int main(int argc, char **argv)
{
  // 增大文件打开数量...
  if( !theApp.doInitRLimit() )
    return -1;
  // 创建udp服务器套接字...
  if( theApp.doCreateSocket(DEF_UDP_PORT) < 0 )
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

// 获取日志全路径...
bool doGetLogPath(char * lpOutPath, int inSize)
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
  // create udpserver.log file...
  strcat(lpOutPath, "udpserver.log");
  return true;
}

// 全新的日志处理函数...
bool do_trace(const char * inFile, int inLine, bool bIsDebug, const char *msg, ...)
{
  // 获取日志全路径...
  doGetLogPath(g_absolute_path, 256);
  if( strlen(g_absolute_path) <= 0 ) {
    fprintf(stderr, "doGetLogPath failed\n");
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
    file_log_fd = open(g_absolute_path, O_RDWR|O_CREAT|O_APPEND, 0666);
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
