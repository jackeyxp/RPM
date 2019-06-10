
#include "app.h"
#include "bmem.h"
#include "getopt.h"
#include <signal.h>
#include <sys/stat.h>
#include <execinfo.h>

// STL must use g++...
// g++ -g udpserver.c bmem.c thread.cpp app.cpp tcpcamera.cpp tcproom.cpp tcpcenter.cpp tcpclient.cpp tcpthread.cpp udpthread.cpp room.cpp network.cpp student.cpp teacher.cpp -o udpserver -lrt -lpthread -ljson
// valgrind --tool=helgrind --tool=memcheck --leak-check=full --show-reachable=yes ./udpserver

CApp theApp;

#define LOG_MAX_SIZE                2048            // 单条日志最大长度(字节)...
#define LOG_MAX_FILE    40 * 1024 * 1024            // 日志文件最大长度(字节)...

char g_absolute_path[256] = {0};
char g_log_file_path[256] = {0};

bool doGetCurFullPath(char * lpOutPath, int inSize);
void doSliceLogFile(struct tm * lpCurTm);
void do_sig_catcher(int signo);
void do_err_crasher(int signo);
bool doRegisterSignal();

int main(int argc, char **argv)
{
  // 获取进程当前运行完整路径...
  if( !doGetCurFullPath(g_absolute_path, 256) )
    return -1;
  // 构造日志文件完整路径...
  sprintf(g_log_file_path, "%s%s", g_absolute_path, DEFAULT_LOG_FILE);
  // 读取命令行各字段内容信息并执行...
  if( theApp.doProcessCmdLine(argc, argv) )
    return -1;
  // 只允许一个进程运行，多个进程会造成混乱...
  if( !theApp.check_pid_file() )
    return -1;
  // 注册信号操作函数...
  if( !doRegisterSignal() )
    return -1;
  // 增大文件打开数量...
  if( !theApp.doInitRLimit() )
    return -1;
  // 创建进程的pid文件...
  if( !theApp.acquire_pid_file() )
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
  // 注意：主线程退出时会删除pid文件...
  // 阻塞循环等待UDP网络数据到达...
  theApp.doWaitUdpSocket();
  // 阻塞终止，退出进程...
  return 0;
}

// 返回全局的App对象...
CApp * GetApp() { return &theApp; }

// 注册全局的信号处理函数...
bool doRegisterSignal()
{
  struct sigaction sa = {0};
  
  /* Install do_sig_catcher() as a signal handler */
  sa.sa_handler = do_sig_catcher;
  sigemptyset(&sa.sa_mask);
  sa.sa_flags = 0;
  sigaction(SIGTERM, &sa, NULL);
 
  sa.sa_handler = do_sig_catcher;
  sigemptyset(&sa.sa_mask);
  sa.sa_flags = 0;
  sigaction(SIGINT, &sa, NULL);
  
  /* Install do_err_crasher() as a signal handler */
  sa.sa_handler = do_err_crasher;
  sigemptyset(&sa.sa_mask);
  sa.sa_flags = 0;
  sigaction(SIGILL, &sa, NULL);   // 非法指令

  sa.sa_handler = do_err_crasher;
  sigemptyset(&sa.sa_mask);
  sa.sa_flags = 0;
  sigaction(SIGBUS, &sa, NULL);   // 总线错误

  sa.sa_handler = do_err_crasher;
  sigemptyset(&sa.sa_mask);
  sa.sa_flags = 0;
  sigaction(SIGFPE, &sa, NULL);   // 浮点异常

  sa.sa_handler = do_err_crasher;
  sigemptyset(&sa.sa_mask);
  sa.sa_flags = 0;
  sigaction(SIGABRT, &sa, NULL);  // 来自abort函数的终止信号

  sa.sa_handler = do_err_crasher;
  sigemptyset(&sa.sa_mask);
  sa.sa_flags = 0;
  sigaction(SIGSEGV, &sa, NULL);  // 无效的存储器引用(段错误)
  
  return true;
}

void do_sig_catcher(int signo)
{
  log_trace("udpserver catch terminate signal=%d", signo);
  if( signo == SIGTERM || signo == SIGINT ) {
    theApp.onSignalQuit();
  }
}

void do_err_crasher(int signo)
{
  // 还原默认的信号处理...
  signal(signo, SIG_DFL);
  // 打印发生崩溃的信号编号...
  log_trace("udpserver catch err-crash signal=%d", signo);
  //////////////////////////////////////////////////////////////////////////
  // 注意：枚举当前调用堆栈，堆栈信息太少，还要增加编译开关...
  // 注意：需要增加编译开关才会有更详细的函数名称 -rdynamic
  // 注意：不记录崩溃堆栈的原因是由于coredump产生的信息更丰富...
  // 注意：崩溃捕获更重要的作用是做善后处理，比如：删除pid文件...
  //////////////////////////////////////////////////////////////////////////
  /*void * DumpArray[256] = {0};
  int nSize = backtrace(DumpArray, 256);
  char ** lppSymbols = backtrace_symbols(DumpArray, nSize);
  for (int i = 0; i < nSize; i++) {
    log_trace("callstack => %d, %s", i, lppSymbols[i]);
  }*/
  // 最后删除pid文件...
  theApp.destory_pid_file();
}

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
    case kCmd_Camera_PTZCommand:    return "Camear_PTZCommand";
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
    case kCmd_PHP_Bind_Mini:        return "PHP_Bind_Mini";
    case kCmd_PHP_GetRoomFlow:      return "PHP_GetRoomFlow";
    case kCmd_Camera_PusherID:      return "Camera_PusherID";
  }
  return "unknown";
}

const char * get_abs_path()
{
  return g_absolute_path;
}

void long2buff(int64_t n, char *buff)
{
	unsigned char *p;
	p = (unsigned char *)buff;
	*p++ = (n >> 56) & 0xFF;
	*p++ = (n >> 48) & 0xFF;
	*p++ = (n >> 40) & 0xFF;
	*p++ = (n >> 32) & 0xFF;
	*p++ = (n >> 24) & 0xFF;
	*p++ = (n >> 16) & 0xFF;
	*p++ = (n >> 8) & 0xFF;
	*p++ = n & 0xFF;
}

int64_t buff2long(const char *buff)
{
	unsigned char *p;
	p = (unsigned char *)buff;
	return  (((int64_t)(*p)) << 56) | \
		(((int64_t)(*(p+1))) << 48) |  \
		(((int64_t)(*(p+2))) << 40) |  \
		(((int64_t)(*(p+3))) << 32) |  \
		(((int64_t)(*(p+4))) << 24) |  \
		(((int64_t)(*(p+5))) << 16) |  \
		(((int64_t)(*(p+6))) << 8) | \
		((int64_t)(*(p+7)));
}
