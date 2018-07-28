
#include "tcpthread.h"
#include <json/json.h>

#define WAIT_TIME_OUT     10 * 1000   // 全局超时检测10秒...
#define MAX_LINE_SIZE     64 * 1024   // 2017.07.25 - by jackey => 避免录像任务命令，造成溢出...
#define CLIENT_TIME_OUT      1 * 60   // 客户端超时断开时间1分钟(汇报频率30秒)...

//
// 获取用户类型...
static const char * get_client_type(int inType)
{
  switch(inType)
  {
    case kClientPHP:     return "PHP";
    case kClientStudent: return "Student";
    case kClientTeacher: return "Teacher";
  }
  return "unknown";
}
//
// 获取命令类型...
static const char * get_command_name(int inCmd)
{
  switch(inCmd)
  {
    case kCmd_Student_Login:            return "Student_Login";
    case kCmd_Student_OnLine:           return "Student_OnLine";
    case kCmd_Teacher_Login:            return "Teacher_Login";
    case kCmd_Teacher_OnLine:           return "Teacher_OnLine";
  }
  return "unknown";
}

CTCPThread::CTCPThread()
  : m_listen_fd(0)
  , m_epoll_fd(0)
{
  // 初始化线程互斥对象...
  pthread_mutex_init(&m_mutex, NULL);
  // 重置epoll事件队列列表...
  memset(m_events, 0, sizeof(epoll_event) * MAX_EPOLL_SIZE);
}

CTCPThread::~CTCPThread()
{
  // 等待线程退出...
  this->StopAndWaitForThread();
  // 先关闭套接字，阻止网络数据到达...
  if( m_listen_fd > 0 ) {
    close(m_listen_fd);
    m_listen_fd = 0;
  }
  // 删除线程互斥对象...
  pthread_mutex_destroy(&m_mutex);  
}

bool CTCPThread::InitThread()
{
  // 创建tcp服务器套接字...
  if( this->doCreateSocket(DEF_TCP_PORT) < 0 )
    return false;
  // 启动tcp服务器监听线程...
  this->Start();
  return true;
}
//
// 创建TCP监听套接字...
int CTCPThread::doCreateSocket(int nHostPort)
{
  // 创建TCP监听套接字...
	int listen_fd = socket(AF_INET, SOCK_STREAM, 0); 
  if( listen_fd < 0 ) {
    log_trace("can't create tcp socket");
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
  // 设置异步套接字 => 失败，关闭套接字...
  if( this->SetNonBlocking(listen_fd) < 0 ) {
    log_trace("SetNonBlocking error: %s", strerror(errno));
    close(listen_fd);
    return -1;
  }
  // 设定发送和接收缓冲最大值...
  int nRecvMaxLen = 64 * 1024;
  int nSendMaxLen = 64 * 1024;
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
	struct sockaddr_in tcpAddr = {0};
	bzero(&tcpAddr, sizeof(tcpAddr));
	tcpAddr.sin_family = AF_INET; 
	tcpAddr.sin_port = htons(nHostPort);
	tcpAddr.sin_addr.s_addr = htonl(INADDR_ANY);
  // 绑定监听端口...
	if( bind(listen_fd, (struct sockaddr *)&tcpAddr, sizeof(struct sockaddr)) == -1 ) {
    log_trace("bind tcp port: %d, error: %s", nHostPort, strerror(errno));
    close(listen_fd);
		return -1;
	}
  // 启动监听队列...
	if( listen(listen_fd, MAX_LISTEN_SIZE) == -1 ) {
    log_trace("listen error: %s", strerror(errno));
    close(listen_fd);
		return -1;
	}
  // create epoll handle, add socket to epoll events...
  // EPOLLEF模式下，accept时必须用循环来接收链接，防止链接丢失...
  struct epoll_event evListen = {0};
	m_epoll_fd = epoll_create(MAX_EPOLL_SIZE);
	evListen.data.fd = listen_fd;
	evListen.events = EPOLLIN | EPOLLET;
	if( epoll_ctl(m_epoll_fd, EPOLL_CTL_ADD, listen_fd, &evListen) < 0 ) {
    log_trace("epoll set insertion error: fd=%d", listen_fd);
		return -1;
	}
  // 返回已经绑定完毕的TCP套接字...
  m_listen_fd = listen_fd;
  return m_listen_fd;
}

int CTCPThread::SetNonBlocking(int sockfd)
{
	if (fcntl(sockfd, F_SETFL, fcntl(sockfd, F_GETFD, 0)|O_NONBLOCK) == -1) {
		return -1;
	}
	return 0;
}

void CTCPThread::Entry()
{
  // 打印TCP监听线程正常启动提示信息...
  log_trace("tcp-server startup, port %d, max-connection is %d, backlog is %d", DEF_TCP_PORT, MAX_EPOLL_SIZE, MAX_LISTEN_SIZE);
  // 进入epoll线程循环过程...
  time_t myStartTime = time(NULL);
	int curfds = 1, acceptCount = 0;
  while( !this->IsStopRequested() ) {
    // 等待epoll事件，直到超时...
    int nfds = epoll_wait(m_epoll_fd, m_events, curfds, WAIT_TIME_OUT);
    // 发生错误，EINTR这个错误，不能退出...
		if( nfds == -1 ) {
      log_trace("epoll_wait error(code:%d, %s)", errno, strerror(errno));
      // is EINTR, continue...
      if( errno == EINTR ) 
        continue;
      // not EINTR, break...
      assert( errno != EINTR );
      break;
		}
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    // 处理超时的情况 => 释放一些已经死掉的资源...
    // 注意：这里的超时，是一种时钟，每隔10秒自动执行一次，不能只处理事件超时，要自行处理...
    // 以前的写法会造成只要2个以上有效用户就永远无法处理超时过程，因为还没超时，就有新事件到达了...
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    time_t nDeltaTime = time(NULL) - myStartTime;
    if( nDeltaTime >= WAIT_TIME_OUT/1000 ) {
      myStartTime = time(NULL);
      this->doHandleTimeout();
    }
    // 刚好是超时事件，继续下次等待...
    if( nfds == 0 )
      continue;
    // 处理正确返回值的情况...
    for(int n = 0; n < nfds; ++n) {
      // 处理服务器socket事件...
      int nCurEventFD = m_events[n].data.fd;
			if( nCurEventFD == m_listen_fd ) {
        // 这里要循环accept链接，可能会有多个链接同时到达...
        while( true ) {
          // 收到客户端连接的socket...
          struct sockaddr_in cliaddr = {0};
          socklen_t socklen = sizeof(struct sockaddr_in);
          int connfd = accept(m_listen_fd, (struct sockaddr *)&cliaddr, &socklen);
          // 发生错误，跳出循环...
          if( connfd < 0 )
            break;
          // eqoll队列超过最大值，关闭，继续...
          if( curfds >= MAX_EPOLL_SIZE ) {
            log_trace("too many connection, more than %d", MAX_EPOLL_SIZE);
            close(connfd);
            break; 
          }
          // set none blocking for the new client socket => error not close... 
          if( this->SetNonBlocking(connfd) < 0 ) {
            log_trace("client SetNonBlocking error fd: %d", connfd);
          }
          // 添加新socket到epoll事件池 => 只加入读取事件...
          struct epoll_event evClient = {0};
          evClient.events = EPOLLIN | EPOLLET;
          evClient.data.fd = connfd;
          // 添加失败，记录，继续...
          if( epoll_ctl(m_epoll_fd, EPOLL_CTL_ADD, connfd, &evClient) < 0 ) {
            log_trace("add socket '%d' to epoll failed: %s", connfd, strerror(errno));
            close(connfd);
            break;
          }
          // 全部成功，打印信息，引用计数增加...
          ++curfds; ++acceptCount;
          int nSinPort = ntohs(cliaddr.sin_port);
          string strSinAddr = inet_ntoa(cliaddr.sin_addr);
          //log_trace("client count(%d) - increase, accept from %s:%d", acceptCount, strSinAddr.c_str(), nSinPort);
          // 创建客户端对象,并保存到集合当中...
          CTCPClient * lpTCPClient = new CTCPClient(this, connfd, nSinPort, strSinAddr);
          m_MapConnect[connfd] = lpTCPClient;
        }
      } else {
        // 处理客户端socket事件...
        int nRetValue = -1;
        if( m_events[n].events & EPOLLIN ) {
          nRetValue = this->doHandleRead(nCurEventFD);
        } else if( m_events[n].events & EPOLLOUT ) {
          nRetValue = this->doHandleWrite(nCurEventFD);
        }
        // 判断处理结果...
        if( nRetValue < 0 ) {
          // 处理失败，从epoll队列中删除...
          struct epoll_event evDelete = {0};
          evDelete.data.fd = nCurEventFD;
          evDelete.events = EPOLLIN | EPOLLET;
          epoll_ctl(m_epoll_fd, EPOLL_CTL_DEL, nCurEventFD, &evDelete);
          // 删除对应的客户端连接对象...
          if( m_MapConnect.find(nCurEventFD) != m_MapConnect.end() ) {
            delete m_MapConnect[nCurEventFD];
            m_MapConnect.erase(nCurEventFD);
          }
          // 关闭连接，减少引用，打印事件...
          close(nCurEventFD);
          --curfds; --acceptCount;
          //log_trace("client count(%d) - decrease", acceptCount);
        }
      }
    }
  }
  // clear all the connected client...
  this->clearAllClient();
  // close listen socket and exit...
  log_trace("tcp-thread exit.");
  close(m_listen_fd);
  m_listen_fd = 0;
}
//
// 删除所有的客户端连接...
void CTCPThread::clearAllClient()
{
  GM_MapConn::iterator itorItem;
  for(itorItem = m_MapConnect.begin(); itorItem != m_MapConnect.end(); ++itorItem) {
    delete itorItem->second;
  }
  m_MapConnect.clear();
}
//
// 处理客户端socket读取事件...
int CTCPThread::doHandleRead(int connfd)
{
  // 在集合中查找客户端对象...
  GM_MapConn::iterator itorConn = m_MapConnect.find(connfd);
  if( itorConn == m_MapConnect.end() ) {
    log_trace("can't find client connection(%d)", connfd);
		return -1;
  }
  // 获取对应的客户端对象，执行读取操作...
  CTCPClient * lpTCPClient = itorConn->second;
  return lpTCPClient->ForRead();  
}
//
// 处理客户端socket写入事件...
int CTCPThread::doHandleWrite(int connfd)
{
  // 在集合中查找客户端对象...
  GM_MapConn::iterator itorConn = m_MapConnect.find(connfd);
  if( itorConn == m_MapConnect.end() ) {
    log_trace("can't find client connection(%d)", connfd);
		return -1;
  }
  // 获取对应的客户端对象，执行写入操作...
  CTCPClient * lpTCPClient = itorConn->second;
  return lpTCPClient->ForWrite();
}
//
// 处理epoll超时事件...
void CTCPThread::doHandleTimeout()
{
  // 2017.07.26 - by jackey => 根据连接状态删除客户端...
  // 2017.12.16 - by jackey => 去掉gettcpstate，使用超时机制...
  // 遍历所有的连接，判断连接是否超时，超时直接删除...
  CTCPClient * lpTCPClient = NULL;
  GM_MapConn::iterator itorConn;
  itorConn = m_MapConnect.begin();
  while( itorConn != m_MapConnect.end() ) {
    lpTCPClient = itorConn->second;
    if( lpTCPClient->IsTimeout() ) {
      // 发生超时，从epoll队列中删除...
      int nCurEventFD = lpTCPClient->m_nConnFD;
      struct epoll_event evDelete = {0};
      evDelete.data.fd = nCurEventFD;
      evDelete.events = EPOLLIN | EPOLLET;
      epoll_ctl(m_epoll_fd, EPOLL_CTL_DEL, nCurEventFD, &evDelete);
      // 打印删除消息，删除对象...
      log_trace("handleTimeout: %s, Socket(%d) be killed", get_client_type(lpTCPClient->m_nClientType), nCurEventFD);
      delete lpTCPClient; lpTCPClient = NULL;
      m_MapConnect.erase(itorConn++);
      // 关闭套接字...
      close(nCurEventFD);
    } else {
      ++itorConn;
    }
  }  
}

CTCPClient::CTCPClient(CTCPThread * lpTCPThread, int connfd, int nHostPort, string & strSinAddr)
  : m_nClientType(0)
  , m_nConnFD(connfd)
  , m_nHostPort(nHostPort)
  , m_strSinAddr(strSinAddr)
  , m_lpTCPThread(lpTCPThread)
{
  assert(m_nConnFD > 0 && m_strSinAddr.size() > 0 );
  assert(m_lpTCPThread != NULL);
  m_nStartTime = time(NULL);
  m_epoll_fd = m_lpTCPThread->GetEpollFD();
}

CTCPClient::~CTCPClient()
{
  // 打印终端退出信息...
  log_debug("Client Delete: %s, From: %s:%d, Socket: %d", get_client_type(m_nClientType), 
            this->m_strSinAddr.c_str(), this->m_nHostPort, this->m_nConnFD);
}
//
// 发送网络数据 => 始终设置读事件...
int CTCPClient::ForWrite()
{
  // 如果没有需要发送的数据，直接返回...
  if( m_strSend.size() <= 0 )
    return 0;
  // 发送全部的数据包内容...
  assert( m_strSend.size() > 0 );
  int nWriteLen = write(m_nConnFD, m_strSend.c_str(), m_strSend.size());
  if( nWriteLen <= 0 ) {
    log_trace("transmit command error(%s)", strerror(errno));
    return -1;
  }
  // 每次发送成功，必须清空发送缓存...
  m_strSend.clear();
  // 准备修改事件需要的数据 => 写事件之后，一定是读事件...
  struct epoll_event evClient = {0};
	evClient.data.fd = m_nConnFD;
  evClient.events = EPOLLIN | EPOLLET;
  // 重新修改事件，加入读取事件...
	if( epoll_ctl(m_epoll_fd, EPOLL_CTL_MOD, m_nConnFD, &evClient) < 0 ) {
    log_trace("mod socket '%d' to epoll failed: %s", m_nConnFD, strerror(errno));
    return -1;
  }
  // 操作成功，返回0...
  return 0;
}
//
// 读取网络数据...
int CTCPClient::ForRead()
{
  // 直接读取网络数据...
	char bufRead[MAX_LINE_SIZE] = {0};
	int  nReadLen = read(m_nConnFD, bufRead, MAX_LINE_SIZE);
  // 读取失败，返回错误，让上层关闭...
  if( nReadLen == 0 ) {
    //log_trace("Client: %s, ForRead: Close, Socket: %d", get_client_type(m_nClientType), this->m_nConnFD);
    return -1;
  }
  // 读取失败，返回错误，让上层关闭...
  if( nReadLen < 0 ) {
		log_trace("Client: %s, read error(%s)", get_client_type(m_nClientType), strerror(errno));
    return -1;
  }
  // 读取数据有效，重置超时时间...
  this->ResetTimeout();
	// 追加读取数据并构造解析头指针...
	m_strRecv.append(bufRead, nReadLen);
	// 这里网络数据会发生粘滞现象，因此，需要循环执行...
	while( m_strRecv.size() > 0 ) {
		// 得到的数据长度不够，直接返回，等待新数据...
    int nCmdLength = sizeof(Cmd_Header);
    if( m_strRecv.size() < nCmdLength )
      return 0;
    // 得到数据的有效长度和指针...
    assert( m_strRecv.size() >= nCmdLength );
    Cmd_Header * lpCmdHeader = (Cmd_Header*)m_strRecv.c_str();
    const char * lpDataPtr = m_strRecv.c_str() + sizeof(Cmd_Header);
    int nDataSize = m_strRecv.size() - sizeof(Cmd_Header);
		// 已获取的数据长度不够，直接返回，等待新数据...
		if( nDataSize < lpCmdHeader->m_pkg_len )
			return 0;
    // 数据区有效，保存用户类型...
    m_nClientType = lpCmdHeader->m_type;
    assert( nDataSize >= lpCmdHeader->m_pkg_len );
    // 判断是否需要解析JSON数据包，解析错误，直接删除链接...
    int nResult = -1;
    if( lpCmdHeader->m_pkg_len > 0 ) {
      nResult = this->parseJsonData(lpDataPtr, lpCmdHeader->m_pkg_len);
      if( nResult < 0 )
        return nResult;
      assert( nResult >= 0 );
    }
    // 打印调试信息到控制台，播放器类型，命令名称，IP地址端口，套接字...
    // 调试模式 => 只打印，不存盘到日志文件...
    log_debug("Client Command(%s - %s, From: %s:%d, Socket: %d)", 
              get_client_type(m_nClientType), get_command_name(lpCmdHeader->m_cmd),
              this->m_strSinAddr.c_str(), this->m_nHostPort, this->m_nConnFD);
    // 对数据进行用户类型分发...
    switch( m_nClientType )
    {
      case kClientPHP:      nResult = this->doPHPClient(lpCmdHeader, lpDataPtr); break;
      case kClientStudent:  nResult = this->doStudentClient(lpCmdHeader, lpDataPtr); break;
      case kClientTeacher:  nResult = this->doTeacherClient(lpCmdHeader, lpDataPtr); break;
    }
		// 删除已经处理完毕的数据 => Header + pkg_len...
		m_strRecv.erase(0, lpCmdHeader->m_pkg_len + sizeof(Cmd_Header));
    // 判断是否已经发生了错误...
    if( nResult < 0 )
      return nResult;
    // 如果没有错误，继续执行...
    assert( nResult >= 0 );
  }  
  return 0;
}

// 处理PHP客户端事件...
int CTCPClient::doPHPClient(Cmd_Header * lpHeader, const char * lpJsonPtr)
{
  return 0;
}

// 处理Student事件...
int CTCPClient::doStudentClient(Cmd_Header * lpHeader, const char * lpJsonPtr)
{
  int nResult = -1;
  switch(lpHeader->m_cmd)
  {
    case kCmd_Student_Login:      nResult = this->doCmdLoginProcess(); break;
    case kCmd_Student_OnLine:     nResult = this->doCmdStudentOnLine(); break;
  }
  return nResult;
}

int CTCPClient::doCmdLoginProcess()
{
  // 处理采集端登录过程 => 判断传递JSON数据有效性...
  if( m_MapJson.find("mac_addr") == m_MapJson.end() ||
    m_MapJson.find("ip_addr") == m_MapJson.end() ||
    m_MapJson.find("room_id") == m_MapJson.end() ) {
    return -1;
  }
  // 保存解析到的有效JSON数据项...
  m_strMacAddr = m_MapJson["mac_addr"];
  m_strIPAddr  = m_MapJson["ip_addr"];
  m_strRoomID  = m_MapJson["room_id"];
  // 不用回复，直接返回...
  return 0;  
}

int CTCPClient::doCmdStudentOnLine()
{
  return 0;
}

// 处理Teacher事件...
int CTCPClient::doTeacherClient(Cmd_Header * lpHeader, const char * lpJsonPtr)
{
  int nResult = -1;
  switch(lpHeader->m_cmd)
  {
    case kCmd_Teacher_Login:        nResult = this->doCmdLoginProcess(); break;
    case kCmd_Teacher_OnLine:       nResult = this->doCmdTeacherOnLine(); break;
  }
  return 0;
}

// 处理Teacher在线汇报命令...
int CTCPClient::doCmdTeacherOnLine()
{
  return 0;
}
//
// 统一的JSON解析接口 => 保存到集合对象当中...
int CTCPClient::parseJsonData(const char * lpJsonPtr, int nJsonLength)
{
  // 首先判断输入数据的有效性...
  if( lpJsonPtr == NULL || nJsonLength <= 0 )
    return -1;
  // 清空上次解析的结果...
  m_MapJson.clear();
  // 解析 JSON 数据包失败，直接返回错误号...
  json_object * new_obj = json_tokener_parse(lpJsonPtr);
  if( new_obj == NULL ) {
    log_trace("parse json data error");
    return -1;
  }
  // check the json type => must be json_type_object...
  json_type nJsonType = json_object_get_type(new_obj);
  if( nJsonType != json_type_object ) {
    log_trace("parse json data error");
    json_object_put(new_obj);
    return -1;
  }
  // 解析传递过来的JSON数据包，存入集合当中...
  json_object_object_foreach(new_obj, key, val) {
    m_MapJson[key] = json_object_get_string(val);
  }
  // 解析数据完毕，释放JSON对象...
  json_object_put(new_obj);
  return 0;
}
//
// 检测是否超时...
bool CTCPClient::IsTimeout()
{
  time_t nDeltaTime = time(NULL) - m_nStartTime;
  return ((nDeltaTime >= CLIENT_TIME_OUT) ? true : false);
}
//
// 重置超时时间...
void CTCPClient::ResetTimeout()
{
  m_nStartTime = time(NULL);
}
