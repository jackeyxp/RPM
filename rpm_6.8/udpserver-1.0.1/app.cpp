
#include "app.h"
#include "room.h"
#include "student.h"
#include "teacher.h"

CApp::CApp()
  : m_listen_fd(0)
  , m_sem_t(NULL)
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

void CApp::doAddSupplyList(CTeacher * lpTeacher)
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

void CApp::doDelSupplyList(CTeacher * lpTeacher)
{
  // 注意：是从doProcSocket()过来的，已经做了互斥处理...
  m_ListTeacher.remove(lpTeacher);
  /*GM_ListTeacher::iterator itorItem;
  itorItem = std::find(m_ListTeacher.begin(), m_ListTeacher.end(), lpTeacher);
  // 如果对象已经存在列表当中，删除之...
  if( itorItem != m_ListTeacher.end() ) {
    m_ListTeacher.erase(itorItem);
  }*/
}

void CApp::Entry()
{
  // 设定默认的信号超时时间 => 100 毫秒...
  unsigned long next_wait_ms = 100;
  uint64_t next_check_ns = os_gettime_ns();
  uint64_t next_detect_ns = next_check_ns;
  while( !this->IsStopRequested() ) {
    // 无论信号量是超时还是被触发，都要执行下面的操作...
    log_trace("[App] start, sem-wait: %d", next_wait_ms);
    if( os_sem_timedwait(m_sem_t, next_wait_ms) == 0 ) {
      log_trace("[App] receive sem notify, wait: %d", next_wait_ms);
    }
    log_trace("[App] end, sem-wait: %d", next_wait_ms);
    // 进行补包对象的补包检测处理 => 返回休息毫秒数...
    next_wait_ms = this->doSendSupply();
    // 等待时间区间 => [0, 100]毫秒...
    assert(next_wait_ms >= 0 && next_wait_ms <= 100);
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

int CApp::doSendSupply()
{
  // 线程互斥锁定 => 是否补过包...
  int n_sleep_ms  = MAX_SLEEP_MS;
  // 补包发送结果 => -1(删除)0(没发)1(已发)...
  int nSendResult = 0;
  pthread_mutex_lock(&m_mutex);  
  GM_ListTeacher::iterator itorItem = m_ListTeacher.begin();
  while( itorItem != m_ListTeacher.end() ) {
    // 执行补包命令，返回执行结果...
    nSendResult = (*itorItem)->doServerSendSupply();
    // -1 => 没有补包了，从列表中删除...
    if( nSendResult < 0 ) {
      m_ListTeacher.erase(itorItem++);
      n_sleep_ms = 100;
      continue;
    }
    // 继续检测下一个有补包的老师推流端...
    ++itorItem;
    // 0 => 有补包，但是不到补包时间 => 休息15毫秒...
    if( nSendResult == 0 ) {
      n_sleep_ms = MAX_SLEEP_MS;
      continue;
    }
    // 1 => 有补包，已经发送补包命令 => 不要休息...
    if( nSendResult > 0 ) {
      n_sleep_ms = 0;
    }
  }
  // 如果队列已经为空 => 休息100毫秒...
  if( m_ListTeacher.size() <= 0 ) {
    n_sleep_ms = 100;
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
CRoom * CApp::doCreateRoom(int inRoomID, int inLiveID)
{
  // 如果找到了房间对象，更新直播通道...
  CRoom * lpRoom = NULL;
  GM_MapRoom::iterator itorRoom = m_MapRoom.find(inRoomID);
  if( itorRoom != m_MapRoom.end() ) {
    lpRoom = itorRoom->second;
    lpRoom->SetLiveID(inLiveID);
    return lpRoom;
  }
  // 如果没有找到房间，创建一个新的房间...
  lpRoom = new CRoom(inRoomID, inLiveID);
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

int CApp::doCreateSocket(int nPort)
{
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
	udpAddr.sin_port = htons(nPort);
	udpAddr.sin_addr.s_addr = htonl(INADDR_ANY);
  // 绑定监听端口...
	if( bind(listen_fd, (struct sockaddr *)&udpAddr, sizeof(struct sockaddr)) == -1 ) {
    log_trace("bind udp port: %d, error: %s", nPort, strerror(errno));
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
    if( nRecvCount == MAX_BUFF_LEN ) {
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
    // 注意：激发重建操作的通常就是探测命令 => 每秒探测一次...
    // 注意：被服务器超时删除之后，才会发生重建...
    // 如果不是创建命令 => 返回重建命令...
    if( ptTag != PT_TAG_CREATE ) {
      rtp_reload_t theReload = {0};
      theReload.tm = TM_TAG_SERVER;
      theReload.id = ID_TAG_SERVER;
      theReload.pt = PT_TAG_RELOAD;
      // 直接返回重建命令包...
      sendto(m_listen_fd, &theReload, sizeof(theReload), 0, (sockaddr*)&inAddr, sizeof(inAddr));
      // 打印重建命令已发出的信息通知...
      log_debug("Server Reload for tmTag: %d, idTag: %d, ptTag: %d", tmTag, idTag, ptTag);
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
