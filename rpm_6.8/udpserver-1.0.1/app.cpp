
#include "app.h"
#include "room.h"
#include "student.h"
#include "teacher.h"
#include "tcpthread.h"
#include <ifaddrs.h>
#include <netdb.h>

CApp::CApp()
  : m_listen_fd(0)
  , m_sem_t(NULL)
  , m_lpTCPThread(NULL)
{
  // 初始化线程互斥对象...
  pthread_mutex_init(&m_mutex, NULL);
  // 初始化辅助线程信号量...
  os_sem_init(&m_sem_t, 0);
}

CApp::~CApp()
{
  // 等待线程退出...
  this->StopAndWaitForThread();
  // 删除TCP线程对象...
  if (m_lpTCPThread != NULL) {
    delete m_lpTCPThread;
    m_lpTCPThread = NULL;
  }
  // 先关闭套接字，阻止网络数据到达...
  if( m_listen_fd > 0 ) {
    close(m_listen_fd);
    m_listen_fd = 0;
  }
  // 释放房间资源...
  GM_MapRoom::iterator itorRoom;
  for(itorRoom = m_MapRoom.begin(); itorRoom != m_MapRoom.end(); ++itorRoom) {
    delete itorRoom->second;
  }
  // 释放网络对象资源...
  GM_MapNetwork::iterator itorItem;
  for(itorItem = m_MapNetwork.begin(); itorItem != m_MapNetwork.end(); ++itorItem) {
    delete itorItem->second;
  }
  // 删除线程互斥对象...
  pthread_mutex_destroy(&m_mutex);
  // 释放辅助线程信号量...
  os_sem_destroy(m_sem_t);
}

void CApp::doLogoutForUDP(int nTCPSockFD, int nDBCameraID, uint8_t tmTag, uint8_t idTag)
{
  if( m_lpTCPThread == NULL )
    return;
  m_lpTCPThread->doLogoutForUDP(nTCPSockFD, nDBCameraID, tmTag, idTag);
}

void CApp::doUDPStudentPusherOnLine(int inRoomID, int inDBCameraID, bool bIsOnLineFlag)
{
  if( m_lpTCPThread == NULL )
    return;
  m_lpTCPThread->doUDPStudentPusherOnLine(inRoomID, inDBCameraID, bIsOnLineFlag);
}

void CApp::doUDPTeacherPusherOnLine(int inRoomID, bool bIsOnLineFlag)
{
  if( m_lpTCPThread == NULL )
    return;
  m_lpTCPThread->doUDPTeacherPusherOnLine(inRoomID, bIsOnLineFlag);
}

bool CApp::IsUDPStudentPusherOnLine(int inRoomID, int inDBCameraID)
{
  // 首先进行线程互斥...
  pthread_mutex_lock(&m_mutex);
  bool bOnLine = false;
  CRoom * lpRoom = NULL;
  CStudent * lpStudent = NULL;
  GM_MapRoom::iterator itorRoom;
  do {
    itorRoom = m_MapRoom.find(inRoomID);
    if( itorRoom == m_MapRoom.end() )
      break;
    lpRoom = itorRoom->second; assert(lpRoom != NULL);
    lpStudent = lpRoom->GetStudentPusher();
    // 房间里没有学生推流者...
    if( lpStudent == NULL )
      break;
    // 检测学生推流者的通道编号是否与输入的通道编号一致...
    bOnLine = ((lpStudent->GetDBCameraID() == inDBCameraID) ? true : false);
  } while( false );
  // 退出互斥，返回查找结果...
  pthread_mutex_unlock(&m_mutex);  
  return bOnLine;  
}

bool CApp::IsUDPTeacherPusherOnLine(int inRoomID)
{
  // 首先进行线程互斥...
  pthread_mutex_lock(&m_mutex);
  bool bOnLine = false;
  CRoom * lpRoom = NULL;
  GM_MapRoom::iterator itorRoom;
  do {
    itorRoom = m_MapRoom.find(inRoomID);
    if( itorRoom == m_MapRoom.end() )
      break;
    lpRoom = itorRoom->second; assert(lpRoom != NULL);
    bOnLine = ((lpRoom->GetTeacherPusher() != NULL) ? true : false);
  } while( false );
  // 退出互斥，返回查找结果...
  pthread_mutex_unlock(&m_mutex);  
  return bOnLine;
}

bool CApp::doStartThread()
{
  // 创建TCP监听对象，并启动线程...
  assert(m_lpTCPThread == NULL);
  m_lpTCPThread = new CTCPThread();
  if (!m_lpTCPThread->InitThread()) {
    log_trace("Init TCPThread failed!");
    return false;
  }
  // 启动自身附加线程...
  this->Start();
  return true;
}

void CApp::doAddSupplyForTeacher(CTeacher * lpTeacher)
{
  // 注意：是从doProcSocket()过来的，已经做了互斥处理...
  GM_ListTeacher::iterator itorItem;
  itorItem = std::find(m_ListTeacher.begin(), m_ListTeacher.end(), lpTeacher);
  // 如果对象已经存在列表当中，直接返回...
  if( itorItem != m_ListTeacher.end() )
    return;
  // 对象没有在列表当中，放到列表尾部...
  m_ListTeacher.push_back(lpTeacher);
  // 通知线程信号量状态发生改变...
  os_sem_post(m_sem_t);
}

void CApp::doDelSupplyForTeacher(CTeacher * lpTeacher)
{
  // 注意：是从doProcSocket()过来的，已经做了互斥处理...
  m_ListTeacher.remove(lpTeacher);
}

void CApp::doAddLoseForStudent(CStudent * lpStudent)
{
  // 注意：是从doProcSocket()过来的，已经做了互斥处理...
  GM_ListStudent::iterator itorItem;
  itorItem = std::find(m_ListStudent.begin(), m_ListStudent.end(), lpStudent);
  // 如果对象已经存在列表当中，直接返回...
  if( itorItem != m_ListStudent.end() )
    return;
  // 对象没有在列表当中，放到列表尾部...
  m_ListStudent.push_back(lpStudent);
  // 通知线程信号量状态发生改变...
  os_sem_post(m_sem_t);
}

void CApp::doDelLoseForStudent(CStudent * lpStudent)
{
  // 注意：是从doProcSocket()过来的，已经做了互斥处理...
  m_ListStudent.remove(lpStudent);
}

void CApp::Entry()
{
  // 设定默认的信号超时时间 => APP_SLEEP_MS 毫秒...
  unsigned long next_wait_ms = APP_SLEEP_MS;
  uint64_t next_check_ns = os_gettime_ns();
  uint64_t next_detect_ns = next_check_ns;
  while( !this->IsStopRequested() ) {
    // 注意：这里用信号量代替sleep的目的是为了避免补包发生时的命令延时...
    // 无论信号量是超时还是被触发，都要执行下面的操作...
    //log_trace("[App] start, sem-wait: %d", next_wait_ms);
    os_sem_timedwait(m_sem_t, next_wait_ms);
    //log_trace("[App] end, sem-wait: %d", next_wait_ms);
    // 进行补包对象的补包检测处理 => 返回休息毫秒数...
    int nRetSupply = this->doSendSupply();
    // 进行学生观看端的丢包处理过程...
    int nRetLose = this->doSendLose();
    // 取两者当中最小的时间做为等待时间...
    next_wait_ms = min(nRetSupply, nRetLose);
    // 等待时间区间 => [0, APP_SLEEP_MS]毫秒...
    assert(next_wait_ms >= 0 && next_wait_ms <= APP_SLEEP_MS);
    // 当前时间与上次检测时间之差 => 转换成秒...
    uint64_t cur_time_ns = os_gettime_ns();
    // 每隔 1 秒服务器向所有终端发起探测命令包...
    if( (cur_time_ns - next_detect_ns)/1000000 >= 1000 ) {
      this->doServerSendDetect();
      next_detect_ns = cur_time_ns;
    }
    // 每隔 10 秒检测一次对象超时 => 检测时间未到，等待半秒再检测...
    int nDeltaSecond = (int)((cur_time_ns- next_check_ns)/1000000000);
    if( nDeltaSecond >= CHECK_TIME_OUT ) {
      this->doCheckTimeout();
      next_check_ns = cur_time_ns;
    }
  }
}

int CApp::doSendLose()
{
  // 线程互斥锁定 => 是否补过包...
  int n_sleep_ms  = APP_SLEEP_MS;
  pthread_mutex_lock(&m_mutex);
  GM_ListStudent::iterator itorItem = m_ListStudent.begin();
  while( itorItem != m_ListStudent.end() ) {
    // 执行发送丢包数据内容，返回是否还要执行丢包...
    bool bSendResult = (*itorItem)->doServerSendLose();
    // true => 还有丢包要发送，不能休息...
    if( bSendResult ) {
      ++itorItem;
      n_sleep_ms = min(n_sleep_ms, 0);
    } else {
      // false => 没有丢包要发了，从队列当中删除...
      m_ListStudent.erase(itorItem++);
      n_sleep_ms = min(n_sleep_ms, APP_SLEEP_MS);
    }
  }  
  // 如果队列已经为空 => 休息100毫秒...
  if( m_ListStudent.size() <= 0 ) {
    n_sleep_ms = APP_SLEEP_MS;
  }
  // 线程互斥解锁...
  pthread_mutex_unlock(&m_mutex);
  // 返回最终计算的休息毫秒数...
  return n_sleep_ms;
}

int CApp::doSendSupply()
{
  // 线程互斥锁定 => 是否补过包...
  int n_sleep_ms  = APP_SLEEP_MS;
  // 补包发送结果 => -1(删除)0(没发)1(已发)...
  pthread_mutex_lock(&m_mutex);  
  GM_ListTeacher::iterator itorItem = m_ListTeacher.begin();
  while( itorItem != m_ListTeacher.end() ) {
    // 执行补包命令，返回执行结果...
    int nSendResult = (*itorItem)->doServerSendSupply();
    // -1 => 没有补包了，从列表中删除...
    if( nSendResult < 0 ) {
      m_ListTeacher.erase(itorItem++);
      n_sleep_ms = min(n_sleep_ms, APP_SLEEP_MS);
      continue;
    }
    // 继续检测下一个有补包的老师推流端...
    ++itorItem;
    // 0 => 有补包，但是不到补包时间 => 休息15毫秒...
    if( nSendResult == 0 ) {
      n_sleep_ms = min(n_sleep_ms, MAX_SLEEP_MS);
      continue;
    }
    // 1 => 有补包，已经发送补包命令 => 不要休息...
    if( nSendResult > 0 ) {
      n_sleep_ms = min(n_sleep_ms, 0);
    }
  }
  // 如果队列已经为空 => 休息100毫秒...
  if( m_ListTeacher.size() <= 0 ) {
    n_sleep_ms = APP_SLEEP_MS;
  }
  // 线程互斥解锁...
  pthread_mutex_unlock(&m_mutex);
  // 返回最终计算的休息毫秒数...
  return n_sleep_ms;
}
//
// 遍历所有对象，发起探测命令...
void CApp::doServerSendDetect()
{
  // 线程互斥锁定...
  pthread_mutex_lock(&m_mutex);  
  CNetwork * lpNetwork = NULL;
  GM_MapNetwork::iterator itorItem = m_MapNetwork.begin();
  while( itorItem != m_MapNetwork.end() ) {
    lpNetwork = itorItem->second;
    lpNetwork->doServerSendDetect();
    ++itorItem;
  }
  // 线程互斥解锁...
  pthread_mutex_unlock(&m_mutex);
}
//
// 遍历对象，进行超时检测...
void CApp::doCheckTimeout()
{
  // 线程互斥锁定...
  pthread_mutex_lock(&m_mutex);  
  // 遍历对象，进行超时检测...
  CNetwork * lpNetwork = NULL;
  GM_MapNetwork::iterator itorItem = m_MapNetwork.begin();
  while( itorItem != m_MapNetwork.end() ) {
    lpNetwork = itorItem->second;
    if( lpNetwork->IsTimeout() ) {
      // 打印删除消息，删除对象 => 在析构函数中对房间信息进行清理工作...
      delete lpNetwork; lpNetwork = NULL;
      m_MapNetwork.erase(itorItem++);
    } else {
      ++itorItem;
    }
  }
  // 线程互斥解锁...
  pthread_mutex_unlock(&m_mutex);
}
//
// 创建房间 => 通过房间号进行创建...
CRoom * CApp::doCreateRoom(int inRoomID)
{
  // 如果找到了房间对象，直接返回...
  CRoom * lpRoom = NULL;
  GM_MapRoom::iterator itorRoom = m_MapRoom.find(inRoomID);
  if( itorRoom != m_MapRoom.end() ) {
    lpRoom = itorRoom->second;
    return lpRoom;
  }
  // 如果没有找到房间，创建一个新的房间...
  lpRoom = new CRoom(inRoomID);
  m_MapRoom[inRoomID] = lpRoom;
  return lpRoom;
}

bool CApp::doInitRLimit()
{
	// set max open file number for one process...
	struct rlimit rt = {0};
	rt.rlim_max = rt.rlim_cur = MAX_OPEN_FILE;
	if( setrlimit(RLIMIT_NOFILE, &rt) == -1 ) {
    log_trace("setrlimit error(%s)", strerror(errno));
		return false;
	}
  return true;
}

// 注意：阿里云专有网络无法获取外网地址，中心服务器可以同链接获取外网地址...
// 因此，这个接口作废了，不会被调用，而是让中心服务器通过链接地址自动获取...
bool CApp::doInitWanAddr()
{
  struct ifaddrs *ifaddr, *ifa;
  char host_ip[NI_MAXHOST] = {0};
  int family, result, is_ok = 0;
  if( getifaddrs(&ifaddr) == -1) {
    return false;
  }
  for( ifa = ifaddr; ifa != NULL; ifa = ifa->ifa_next ) { 
    if( ifa->ifa_addr == NULL )
      continue;
    family = ifa->ifa_addr->sa_family;
    if( family != AF_INET )
      continue;
    result = getnameinfo(ifa->ifa_addr, sizeof(struct sockaddr_in), host_ip, NI_MAXHOST, NULL, 0, NI_NUMERICHOST);
    if( result != 0 )
      continue;
    // 先排除本机的环路地址...
    if( strcasecmp(host_ip, "127.0.0.1") == 0 )
      continue;
    // 将获取的IP地址进行转换判断...
    uint32_t nHostAddr = ntohl(inet_addr(host_ip));
    // 检查是否是以下三类内网地址...
    // A类：10.0.0.0 ~ 10.255.255.255
    // B类：172.16.0.0 ~ 172.31.255.255
    // C类：192.168.0.0 ~ 192.168.255.255
    if((nHostAddr >= 0x0A000000 && nHostAddr <= 0x0AFFFFFF) ||
      (nHostAddr >= 0xAC100000 && nHostAddr <= 0xAC1FFFFF) ||
      (nHostAddr >= 0xC0A80000 && nHostAddr <= 0xC0A8FFFF))
      continue;
    // 不是三类内网地址，说明找到了本机的外网地址...
    is_ok = 1;
    break;
  }
  // 释放资源，没有找到，直接返回...
  freeifaddrs(ifaddr);
  if( !is_ok ) {
    return false;
  }
  // 如果汇报地址host_ip为空，打印错误，返回...
  if( strlen(host_ip) <= 0 ) {
    log_trace("Error: host_ip is empty ==");
    return false;
  }
  // 保存外网地址...
  m_strWanAddr = host_ip;
  return true;
}

int CApp::doCreateUdpSocket()
{
  // 获取UDP监听端口配置...
  int nUdpPort = this->GetUdpPort();
  // 创建UDP监听套接字...
	int listen_fd = socket(AF_INET, SOCK_DGRAM, 0); 
  if( listen_fd < 0 ) {
    log_trace("can't create udp socket");
    return -1;
  }
 	int opt = 1;
  // 设置地址重用 => 失败，关闭套接字...
  if( setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) != 0 ) {
    log_trace("SO_REUSEADDR error: %s", strerror(errno));
    close(listen_fd);
    return -1;
  }
  // 设置端口重用 => 失败，关闭套接字...
  if( setsockopt(listen_fd, SOL_SOCKET, SO_REUSEPORT, &opt, sizeof(opt)) != 0 ) {
    log_trace("SO_REUSEPORT error: %s", strerror(errno));
    close(listen_fd);
    return -1;
  }
  // 设定发送和接收缓冲最大值...
  int nRecvMaxLen = 256 * 1024;
  int nSendMaxLen = 256 * 1024;
  // 设置接收缓冲区...
  if( setsockopt(listen_fd, SOL_SOCKET, SO_RCVBUF, &nRecvMaxLen, sizeof(nRecvMaxLen)) != 0 ) {
    log_trace("SO_RCVBUF error: %s", strerror(errno));
    close(listen_fd);
    return -1;
  }
  // 设置发送缓冲区...
  if( setsockopt(listen_fd, SOL_SOCKET, SO_SNDBUF, &nSendMaxLen, sizeof(nSendMaxLen)) != 0 ) {
    log_trace("SO_SNDBUF error: %s", strerror(errno));
    close(listen_fd);
    return -1;
  }
  // 准备绑定地址结构体...
	struct sockaddr_in udpAddr = {0};
	bzero(&udpAddr, sizeof(udpAddr));
	udpAddr.sin_family = AF_INET; 
	udpAddr.sin_port = htons(nUdpPort);
	udpAddr.sin_addr.s_addr = htonl(INADDR_ANY);
  // 绑定监听端口...
	if( bind(listen_fd, (struct sockaddr *)&udpAddr, sizeof(struct sockaddr)) == -1 ) {
    log_trace("bind udp port: %d, error: %s", nUdpPort, strerror(errno));
    close(listen_fd);
		return -1;
	}
  // 返回已经绑定完毕的UDP套接字...
  m_listen_fd = listen_fd;
  return m_listen_fd;
}

void CApp::doWaitSocket()
{
  struct sockaddr_in recvAddr = {0};
  char recvBuff[MAX_BUFF_LEN] = {0};
  int nAddrLen = 0, nRecvCount = 0;
  while ( m_listen_fd > 0 ) {
    // 从网络层阻塞接收UDP数据报文...
    bzero(recvBuff, MAX_BUFF_LEN);
    nAddrLen = sizeof(recvAddr);
    nRecvCount = recvfrom(m_listen_fd, recvBuff, MAX_BUFF_LEN, 0, (sockaddr*)&recvAddr, (socklen_t*)&nAddrLen);
    /////////////////////////////////////////////////////////////////////////////////////
    // 如果返回长度与输入长度一致 => 说明发送端数据越界 => 超过了系统实际处理长度...
    // 注意：出现这种情况，一定要排查发送端的问题 => 通常是序号越界造成的...
    /////////////////////////////////////////////////////////////////////////////////////
    int nMaxSize = DEF_MTU_SIZE + sizeof(rtp_hdr_t);
    if( nRecvCount > nMaxSize ) {
      log_debug("Error Packet Excessed");
      continue;
    }
    // 发生错误，打印并退出...
    if( nRecvCount <= 0 ) {
      log_trace("recvfrom error(code:%d, %s)", errno, strerror(errno));
      // is EINTR, continue...
      if( errno == EINTR ) 
        continue;
      // not EINTR, break...
      assert( errno != EINTR );
      break;
    }
    // 线程互斥锁定...
    pthread_mutex_lock(&m_mutex);  
    // 处理网络数据到达事件 => 为了使用线程互斥...
    this->doProcSocket(recvBuff, nRecvCount, recvAddr);
    // 线程互斥解锁...
    pthread_mutex_unlock(&m_mutex);
  }
}

bool CApp::doProcSocket(char * lpBuffer, int inBufSize, sockaddr_in & inAddr)
{
  // 通过第一个字节的低2位，判断终端类型...
  uint8_t tmTag = lpBuffer[0] & 0x03;
  // 获取第一个字节的中2位，得到终端身份...
  uint8_t idTag = (lpBuffer[0] >> 2) & 0x03;
  // 获取第一个字节的高4位，得到数据包类型...
  uint8_t ptTag = (lpBuffer[0] >> 4) & 0x0F;

  //////////////////////////////////////////////////////////////////////////////////////////////////////////////
  // 打印调试信息 => 打印所有接收到的数据包内容格式信息...
  //log_debug("recvfrom, size: %u, tmTag: %d, idTag: %d, ptTag: %d", inBufSize, tmTag, idTag, ptTag);
  //////////////////////////////////////////////////////////////////////////////////////////////////////////////

  // 如果终端既不是Student也不是Teacher或Server，错误终端，直接扔掉数据...
  if( tmTag != TM_TAG_STUDENT && tmTag != TM_TAG_TEACHER && tmTag != TM_TAG_SERVER ) {
    log_debug("Error Terminate Type: %d", tmTag);
    return false;
  }
  // 如果终端身份既不是Pusher也不是Looker或Server，错误身份，直接扔掉数据...
  if( idTag != ID_TAG_PUSHER && idTag != ID_TAG_LOOKER && idTag != ID_TAG_SERVER ) {
    log_debug("Error Identify Type: %d", idTag);
    return false;
  }
  // 获取发送者映射的地址和端口号 => 后期需要注意端口号变化的问题...
  uint32_t nHostSinAddr = ntohl(inAddr.sin_addr.s_addr);
  uint16_t nHostSinPort = ntohs(inAddr.sin_port);
  // 如果是删除指令，需要做特殊处理...
  if( ptTag == PT_TAG_DELETE ) {
    this->doTagDelete(nHostSinPort);
    return false;
  }
  // 通过获得的端口，查找CNetwork对象...
  CNetwork * lpNetwork = NULL;
  GM_MapNetwork::iterator itorItem;
  itorItem = m_MapNetwork.find(nHostSinPort);
  // 如果没有找到创建一个新的对象...
  if( itorItem == m_MapNetwork.end() ) {
    // 如果不是创建命令 => 打印错误信息...
    if( ptTag != PT_TAG_CREATE ) {
      log_debug("Server Reject for tmTag: %d, idTag: %d, ptTag: %d", tmTag, idTag, ptTag);
      return false;
    }
    assert( ptTag == PT_TAG_CREATE );
    // 只有创建命令可以被用来创建新对象...
    switch( tmTag ) {
      case TM_TAG_STUDENT: lpNetwork = new CStudent(tmTag, idTag, nHostSinAddr, nHostSinPort); break;
      case TM_TAG_TEACHER: lpNetwork = new CTeacher(tmTag, idTag, nHostSinAddr, nHostSinPort); break;
    }
  } else {
    // 注意：这里可能会连续收到 PT_TAG_CREATE 命令，不影响...
    lpNetwork = itorItem->second;
    assert( lpNetwork->GetHostAddr() == nHostSinAddr );
    assert( lpNetwork->GetHostPort() == nHostSinPort );
    // 注意：探测包的tmTag和idTag，可能与对象的tmTag和idTag不一致 => 探测包可能是转发的，身份是相反的...
  }
  // 如果网络层对象为空，打印错误...
  if( lpNetwork == NULL ) {
    log_trace("Error CNetwork is NULL");
    return false;
  }
  // 将网络对象更新到对象集合当中...
  m_MapNetwork[nHostSinPort] = lpNetwork;
  // 将获取的数据包投递到网络对象当中...
  return lpNetwork->doProcess(ptTag, lpBuffer, inBufSize);
}

void CApp::doTagDelete(int nHostPort)
{
  // 通过获得的端口，查找CNetwork对象...
  CNetwork * lpNetwork = NULL;
  GM_MapNetwork::iterator itorItem;
  itorItem = m_MapNetwork.find(nHostPort);
  if( itorItem == m_MapNetwork.end() ) {
    log_debug("Delete can't find CNetwork by host port");
    return;
  }
  // 将找到的CNetwork对象删除之...
  lpNetwork = itorItem->second;
  delete lpNetwork; lpNetwork = NULL;
  m_MapNetwork.erase(itorItem++);  
}

// 处理由kCmd_Camera_LiveStop触发的删除事件...
void CApp::doDeleteForCameraLiveStop(int inRoomID)
{
  // 首先进行线程互斥...
  pthread_mutex_lock(&m_mutex);
  CRoom * lpUdpRoom = NULL;
  CStudent * lpStudentPusher = NULL;
  CTeacher * lpTeacherLooker = NULL;
  do {
    // 通过指定的房间编号查找UDP房间对象...
    GM_MapRoom::iterator itorRoom = m_MapRoom.find(inRoomID);
    if( itorRoom == m_MapRoom.end() )
      break;
    // 获取UDP房间里的唯一老师观看者对象和唯一学生推流者对象...
    lpUdpRoom = itorRoom->second; assert(lpUdpRoom != NULL);
    lpTeacherLooker = lpUdpRoom->GetTeacherLooker();
    lpStudentPusher = lpUdpRoom->GetStudentPusher();
    // 房间里的老师观看者对象有效，删除之...
    if( lpTeacherLooker != NULL ) {
      // 设置删除标志，不要通知老师端，避免死锁...
      lpTeacherLooker->SetDeleteByTCP();
      // 通过关联端口号，删除老师观看者对象...
      this->doTagDelete(lpTeacherLooker->GetHostPort());
    }
    // 房间里的学生推流者对象有效，删除之...
    if( lpStudentPusher != NULL ) {
      // 设置删除标志，不要通知学生端，避免死锁...
      lpStudentPusher->SetDeleteByTCP();
      // 通过关联端口号，删除学生推流者对象...
      this->doTagDelete(lpStudentPusher->GetHostPort());
    }
  } while( false );  
  // 最后退出线程互斥...
  pthread_mutex_unlock(&m_mutex);    
}
