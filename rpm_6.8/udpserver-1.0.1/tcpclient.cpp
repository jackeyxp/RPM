
#include "app.h"
#include "tcproom.h"
#include "tcpclient.h"
#include "tcpthread.h"
#include <json/json.h>

#define MAX_PATH_SIZE           260
#define MAX_LINE_SIZE     64 * 1024   // 2017.07.25 - by jackey => 避免录像任务命令，造成溢出...
#define CLIENT_TIME_OUT      1 * 60   // 客户端超时断开时间1分钟(汇报频率30秒)...

CTCPClient::CTCPClient(CTCPThread * lpTCPThread, int connfd, int nHostPort, string & strSinAddr)
  : m_nRoomID(0)
  , m_nClientType(0)
  , m_nConnFD(connfd)
  , m_lpTCPRoom(NULL)
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
  // 如果是学生端，从房间当中删除之...
  if( m_lpTCPRoom != NULL && m_nClientType == kClientStudent ) {
    m_lpTCPRoom->doDeleteStudent(this);
  }
  // 如果是讲师端，从房间当中删除之...
  if( m_lpTCPRoom != NULL && m_nClientType == kClientTeacher ) {
    m_lpTCPRoom->doDeleteTeacher(this);
  }
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
    case kCmd_Student_Login:      nResult = this->doCmdStudentLogin(); break;
    case kCmd_Student_OnLine:     nResult = this->doCmdStudentOnLine(); break;
  }
  return nResult;
}

// 处理Student登录事件...
int CTCPClient::doCmdStudentLogin()
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
  m_nRoomID = atoi(m_strRoomID.c_str());
  // 创建或更新房间，更新房间里的学生端...
  m_lpTCPRoom = m_lpTCPThread->doCreateRoom(m_nRoomID);
  m_lpTCPRoom->doCreateStudent(this);
  // 当前房间里的TCP讲师端是否在线 和 UDP讲师端是否在线...
  bool bIsTCPTeacherOnLine = ((m_lpTCPRoom->GetTCPTeacher() != NULL) ? true : false);
  bool bIsUDPTeacherOnLine = GetApp()->IsUDPTeacherPusherOnLine(m_nRoomID);
  // 发送反馈命令信息给学生端...
  return this->doSendCmdLoginForStudent(bIsTCPTeacherOnLine, bIsUDPTeacherOnLine);
}

int CTCPClient::doSendCmdLoginForStudent(bool bIsTCPOnLine, bool bIsUDPOnLine)
{
  // 构造转发JSON数据块...
  char szSendBuf[MAX_PATH_SIZE] = {0};
  json_object * new_obj = json_object_new_object();
  json_object_object_add(new_obj, "tcp_teacher", json_object_new_int(bIsTCPOnLine));
  json_object_object_add(new_obj, "udp_teacher", json_object_new_int(bIsUDPOnLine));
  // 转换成json字符串，获取字符串长度...
  char * lpNewJson = (char*)json_object_to_json_string(new_obj);
  int nBodyLen = strlen(lpNewJson);
  // 构造回复结构头信息...
  Cmd_Header theHeader = {0};
  theHeader.m_type = kClientStudent;
  theHeader.m_cmd  = kCmd_Student_Login;
  theHeader.m_pkg_len = nBodyLen;
  memcpy(szSendBuf, &theHeader, sizeof(theHeader));
  memcpy(szSendBuf+sizeof(theHeader), lpNewJson, nBodyLen);
  // 向学生端对象发送组合后的数据包...
  assert( m_nClientType == kClientStudent );
  m_strSend.assign(szSendBuf, nBodyLen+sizeof(theHeader));
  // json对象引用计数减少...
  json_object_put(new_obj);
  // 向学生端对象发起发送数据事件...
  epoll_event evClient = {0};
  evClient.data.fd = m_nConnFD;
  evClient.events = EPOLLOUT | EPOLLET;
  // 重新修改事件，加入写入事件...
  if( epoll_ctl(m_epoll_fd, EPOLL_CTL_MOD, m_nConnFD, &evClient) < 0 ) {
    log_trace("mod socket '%d' to epoll failed: %s", m_nConnFD, strerror(errno));
    return -1;
  }
  // 返回执行正确...
  return 0;
}

void CTCPClient::doUDPTeacherPusherOnLine(bool bIsOnLineFlag)
{
  // 如果不是学生端对象，直接返回...
  if( m_nClientType != kClientStudent )
    return;
  // 当前房间里的TCP讲师端是否在线 和 UDP讲师端是否在线...
  bool bIsTCPTeacherOnLine = ((m_lpTCPRoom != NULL && m_lpTCPRoom->GetTCPTeacher() != NULL) ? true : false);
  bool bIsUDPTeacherOnLine = bIsOnLineFlag;
  // 向本学生端转发登录成功命令通知...
  this->doSendCmdLoginForStudent(bIsTCPTeacherOnLine, bIsUDPTeacherOnLine);
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
    case kCmd_Teacher_Login:        nResult = this->doCmdTeacherLogin(); break;
    case kCmd_Teacher_OnLine:       nResult = this->doCmdTeacherOnLine(); break;
  }
  return 0;
}

// 处理Teacher登录事件...
int CTCPClient::doCmdTeacherLogin()
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
  m_nRoomID = atoi(m_strRoomID.c_str());
  // 创建或更新房间，更新房间里的讲师端...
  m_lpTCPRoom = m_lpTCPThread->doCreateRoom(m_nRoomID);
  m_lpTCPRoom->doCreateTeacher(this);
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
