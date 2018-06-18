
#include "app.h"
#include "room.h"
#include "student.h"
#include "teacher.h"

CApp::CApp()
  : m_listen_fd(0)
{
  // 初始化线程互斥对象...
  pthread_mutex_init(&m_mutex, NULL);
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
}

void CApp::Entry()
{
  uint64_t next_check_ns = os_gettime_ns();
  while( !this->IsStopRequested() ) {
    // 当前时间与上次检测时间之差 => 转换成秒...
    uint64_t cur_time_ns = os_gettime_ns();
    int nDeltaSecond = (int)((cur_time_ns- next_check_ns)/1000000000);
    // 每隔 10 秒检测一次对象超时 => 检测时间未到，等待半秒再检测...
    if( nDeltaSecond < CHECK_TIME_OUT ) {
      os_sleep_ms(500);
      continue;
    }
    // 遍历对象，进行超时检测...
    this->doCheckTimeout();
    next_check_ns = cur_time_ns;
  }
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
  int nAddrLen = sizeof(recvAddr);
  int nRecvCount = 0;
  while ( m_listen_fd > 0 ) {
    // 从网络层阻塞接收UDP数据报文...
    bzero(recvBuff, MAX_BUFF_LEN);
    nRecvCount = recvfrom(m_listen_fd, recvBuff, MAX_BUFF_LEN, 0, (sockaddr*)&recvAddr, (socklen_t*)&nAddrLen);
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

bool CApp::doProcSocket(char * recvBuff, int nRecvCount, sockaddr_in & recvAddr)
{
  // 通过第一个字节的低2位，判断终端类型...
  uint8_t tmTag = recvBuff[0] & 0x03;
  // 获取第一个字节的中2位，得到终端身份...
  uint8_t idTag = (recvBuff[0] >> 2) & 0x03;
  // 获取第一个字节的高4位，得到数据包类型...
  uint8_t ptTag = (recvBuff[0] >> 4) & 0x0F;
  // 如果终端既不是Student也不是Teacher，错误终端，直接扔掉数据...
  if( tmTag != TM_TAG_STUDENT && tmTag != TM_TAG_TEACHER ) {
    log_debug("Error Terminate Type: %d", tmTag);
    return false;
  }
  // 如果终端身份既不是Pusher也不是Looker，错误身份，直接扔掉数据...
  if( idTag != ID_TAG_PUSHER && idTag != ID_TAG_LOOKER ) {
    log_debug("Error Identify Type: %d", idTag);
    return false;
  }
  // 获取发送者映射的地址和端口号 => 后期需要注意端口号变化的问题...
  uint32_t nHostSinAddr = ntohl(recvAddr.sin_addr.s_addr);
  uint16_t nHostSinPort = ntohs(recvAddr.sin_port);
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
    // 不能用探测包进行对象创建 => 探测包可能是转发的，身份是相反的...
    if( ptTag == PT_TAG_DETECT ) {
      log_debug("Detect can't be used to new CNetwork");
      return false;
    }
    // 只有非探测包可以被用来创建新对象...
    switch( tmTag ) {
      case TM_TAG_STUDENT: lpNetwork = new CStudent(tmTag, idTag, nHostSinAddr, nHostSinPort); break;
      case TM_TAG_TEACHER: lpNetwork = new CTeacher(tmTag, idTag, nHostSinAddr, nHostSinPort); break;
    }
  } else {
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
  return lpNetwork->doProcess(ptTag, recvBuff, nRecvCount);
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
