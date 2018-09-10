
#include "app.h"
#include "tcpclient.h"
#include "tcpthread.h"
#include <json/json.h>

#define MAX_PATH_SIZE           260
#define MAX_LINE_SIZE          4096   // 最大命令缓存长度，以前是64K，太长了...
#define CLIENT_TIME_OUT      1 * 60   // 客户端超时断开时间1分钟(汇报频率30秒)...

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
  log_trace("Client Delete: %s, From: %s:%d, Socket: %d", get_client_type(m_nClientType), 
            this->m_strSinAddr.c_str(), this->m_nHostPort, this->m_nConnFD);
  // 终端退出时，需要删除服务器对象...
  GetApp()->doDeleteUdpServer(m_nConnFD);
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
    log_trace("Client Command(%s - %s, From: %s:%d, Socket: %d)", 
              get_client_type(m_nClientType), get_command_name(lpCmdHeader->m_cmd),
              this->m_strSinAddr.c_str(), this->m_nHostPort, this->m_nConnFD);
    // 对数据进行用户类型分发...
    switch( m_nClientType )
    {
      case kClientPHP:        nResult = this->doPHPClient(lpCmdHeader, lpDataPtr); break;
      case kClientUdpServer:  nResult = this->doUdpServerClient(lpCmdHeader, lpDataPtr); break;
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
  int nResult = -1;
  switch(lpHeader->m_cmd)
  {
    case kCmd_PHP_GetUdpServer:   nResult = this->doCmdPHPGetUdpServer(); break;
  }
  // 默认全部返回正确...
  return 0;
}

// 处理PHP发送的根据房间号查询UDP服务器的事件...
int CTCPClient::doCmdPHPGetUdpServer()
{
  // 准备反馈需要的变量...
  int nErrCode = ERR_OK;
  // 创建反馈的json数据包...
  json_object * new_obj = json_object_new_object();
  do {
    // 判断传递JSON数据有效性 => 必须包含room_id字段信息...
    if( m_MapJson.find("room_id") == m_MapJson.end() ) {
      nErrCode = ERR_NO_ROOM;
      break;
    }
    // 获取传递过来的房间编号...
    int nRoomID = atoi(m_MapJson["room_id"].c_str());
    // 通过房间号查找房间的方式，间接查找直播服务器...
    CTCPRoom * lpTCPRoom = GetApp()->doFindTCPRoom(nRoomID);
    CUdpServer * lpUdpServer = ((lpTCPRoom != NULL) ? lpTCPRoom->GetUdpServer() : NULL);
    // 如果直播服务器无效，查找挂载量最小的直播服务器...
    if( lpUdpServer == NULL ) {
      lpUdpServer = GetApp()->doFindMinUdpServer();
    }
    // 如果最终还是没有找到直播服务器，设置错误编号...
    if( lpUdpServer == NULL ) {
      nErrCode = ERR_NO_SERVER;
      break;
    }
    // 组合PHP需要的直播服务器结果信息...
    json_object_object_add(new_obj, "remote_addr", json_object_new_string(lpUdpServer->m_strRemoteAddr.c_str()));
    json_object_object_add(new_obj, "remote_port", json_object_new_int(lpUdpServer->m_nRemotePort));
    json_object_object_add(new_obj, "udp_addr", json_object_new_string(lpUdpServer->m_strUdpAddr.c_str()));
    json_object_object_add(new_obj, "udp_port", json_object_new_int(lpUdpServer->m_nUdpPort));
    // 组合PHP需要的查找通道的内容信息...
    int nTeacherCount = ((lpTCPRoom != NULL) ? lpTCPRoom->GetTeacherCount() : 0);
    int nStudentCount = ((lpTCPRoom != NULL) ? lpTCPRoom->GetStudentCount() : 0);
    json_object_object_add(new_obj, "room_id", json_object_new_int(nRoomID));
    json_object_object_add(new_obj, "teacher", json_object_new_int(nTeacherCount));
    json_object_object_add(new_obj, "student", json_object_new_int(nStudentCount));
  } while( false );
  // 组合错误信息内容...
  json_object_object_add(new_obj, "err_code", json_object_new_int(nErrCode));
  json_object_object_add(new_obj, "err_cmd", json_object_new_int(kCmd_PHP_GetUdpServer));
  // 转换成json字符串，获取字符串长度...
  char * lpNewJson = (char*)json_object_to_json_string(new_obj);
  // 使用统一的通用命令发送接口函数...
  int nResult = this->doSendPHPResponse(lpNewJson, strlen(lpNewJson));
  // json对象引用计数减少...
  json_object_put(new_obj);
  // 返回执行结果...
  return nResult;
}

// 处理UdpServer事件...
int CTCPClient::doUdpServerClient(Cmd_Header * lpHeader, const char * lpJsonPtr)
{
  int nResult = -1;
  switch(lpHeader->m_cmd)
  {
    case kCmd_UdpServer_Login:      nResult = this->doCmdUdpServerLogin(); break;
    case kCmd_UdpServer_OnLine:     nResult = this->doCmdUdpServerOnLine(); break;
    case kCmd_UdpServer_AddTeacher: nResult = this->doCmdUdpServerAddTeacher(); break;
    case kCmd_UdpServer_DelTeacher: nResult = this->doCmdUdpServerDelTeacher(); break;
    case kCmd_UdpServer_AddStudent: nResult = this->doCmdUdpServerAddStudent(); break;
    case kCmd_UdpServer_DelStudent: nResult = this->doCmdUdpServerDelStudent(); break;
  }
  // 默认全部返回正确...
  return 0;
}

// 处理UdpServer的登录事件...
int CTCPClient::doCmdUdpServerLogin()
{
  // 判断传递JSON数据有效性 => 必须包含服务器地址|端口字段信息...
  if( m_MapJson.find("remote_addr") == m_MapJson.end() ||
    m_MapJson.find("remote_port") == m_MapJson.end() ||
    m_MapJson.find("udp_addr") == m_MapJson.end() ||
    m_MapJson.find("udp_port") == m_MapJson.end() ) {
    return -1;
  }
  // 创建或更新服务器对象，创建成功，更新信息...
  CUdpServer * lpUdpServer = GetApp()->doCreateUdpServer(m_nConnFD);
  if( lpUdpServer != NULL ) {
    lpUdpServer->m_strRemoteAddr = m_MapJson["remote_addr"];
    lpUdpServer->m_strUdpAddr = m_MapJson["udp_addr"];
    lpUdpServer->m_nUdpPort = atoi(m_MapJson["udp_port"].c_str());
    lpUdpServer->m_nRemotePort = atoi(m_MapJson["remote_port"].c_str());
    log_trace("[UdpServer] UdpAddr => %s:%d, RemoteAddr => %s:%d",
              lpUdpServer->m_strUdpAddr.c_str(), lpUdpServer->m_nUdpPort,
              lpUdpServer->m_strRemoteAddr.c_str(), lpUdpServer->m_nRemotePort);
  }
  return 0;
}

// 处理UdpServer的在线汇报事件...
int CTCPClient::doCmdUdpServerOnLine()
{
  return 0;
}

// 处理UdpServer汇报的添加老师命令...
int CTCPClient::doCmdUdpServerAddTeacher()
{
  // 判断传递JSON数据有效性 => 必须包含room_id字段信息...
  if( m_MapJson.find("room_id") == m_MapJson.end() ) {
    return -1;
  }
  // 通过套接字编号查找服务器对象，没有找到，直接返回...
  CUdpServer * lpUdpServer = GetApp()->doFindUdpServer(m_nConnFD);
  if( lpUdpServer == NULL )
    return -1;
  // 通知服务器对象，指定房间里的教师引用计数增加...
  int nRoomID = atoi(m_MapJson["room_id"].c_str());
  lpUdpServer->doAddTeacher(nRoomID);
  return 0;
}

// 处理UdpServer汇报的删除老师命令...
int CTCPClient::doCmdUdpServerDelTeacher()
{
  // 判断传递JSON数据有效性 => 必须包含room_id字段信息...
  if( m_MapJson.find("room_id") == m_MapJson.end() ) {
    return -1;
  }
  // 通过套接字编号查找服务器对象，没有找到，直接返回...
  CUdpServer * lpUdpServer = GetApp()->doFindUdpServer(m_nConnFD);
  if( lpUdpServer == NULL )
    return -1;
  // 通知服务器对象，指定房间里的教师引用计数减少...
  int nRoomID = atoi(m_MapJson["room_id"].c_str());
  lpUdpServer->doDelTeacher(nRoomID);
  return 0;
}

// 处理UdpServer汇报的添加学生命令...
int CTCPClient::doCmdUdpServerAddStudent()
{
  // 判断传递JSON数据有效性 => 必须包含room_id字段信息...
  if( m_MapJson.find("room_id") == m_MapJson.end() ) {
    return -1;
  }
  // 通过套接字编号查找服务器对象，没有找到，直接返回...
  CUdpServer * lpUdpServer = GetApp()->doFindUdpServer(m_nConnFD);
  if( lpUdpServer == NULL )
    return -1;
  // 通知服务器对象，指定房间里的学生引用计数增加...
  int nRoomID = atoi(m_MapJson["room_id"].c_str());
  lpUdpServer->doAddStudent(nRoomID);
  return 0;
}

// 处理UdpServer汇报的删除学生命令...
int CTCPClient::doCmdUdpServerDelStudent()
{
  // 判断传递JSON数据有效性 => 必须包含room_id字段信息...
  if( m_MapJson.find("room_id") == m_MapJson.end() ) {
    return -1;
  }
  // 通过套接字编号查找服务器对象，没有找到，直接返回...
  CUdpServer * lpUdpServer = GetApp()->doFindUdpServer(m_nConnFD);
  if( lpUdpServer == NULL )
    return -1;
  // 通知服务器对象，指定房间里的学生引用计数减少...
  int nRoomID = atoi(m_MapJson["room_id"].c_str());
  lpUdpServer->doDelStudent(nRoomID);
  return 0;
}

// PHP专用的反馈接口函数...
int CTCPClient::doSendPHPResponse(const char * lpJsonPtr, int nJsonSize)
{
  // PHP反馈需要的是TrackerHeader结构体...
  TrackerHeader theTracker = {0};
  char szSendBuf[MAX_LINE_SIZE] = {0};
  int nBodyLen = nJsonSize;
  // 组合TrackerHeader包头，方便php扩展使用 => 只需要设置pkg_len，其它置0...
  long2buff(nBodyLen, theTracker.pkg_len);
  memcpy(szSendBuf, &theTracker, sizeof(theTracker));
  memcpy(szSendBuf+sizeof(theTracker), lpJsonPtr, nBodyLen);
  // 将发送数据包缓存起来，等待发送事件到来...
  m_strSend.assign(szSendBuf, nBodyLen+sizeof(theTracker));
  // 向当前终端对象发起发送数据事件...
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
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// 注意：目前这种通过m_strSend中转发送缓存，存在一定的风险，多个线程同时发送可能就会发生命令丢失的问题...
// 注意：判断的标准是m_strSend是否为空，不为空，说明数据还没有被发走，因此，这里需要改进，改动比较大...
// 注意：epoll_event.data 是union类型，里面的4个变量不能同时使用，只能使用一个，目前我们用的是fd
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// 统一的通用命令发送接口函数...
int CTCPClient::doSendCommonCmd(int nCmdID, const char * lpJsonPtr/* = NULL*/, int nJsonSize/* = 0*/)
{
  // 构造回复结构头信息...
  Cmd_Header theHeader = {0};
  theHeader.m_pkg_len = ((lpJsonPtr != NULL) ? nJsonSize : 0);
  theHeader.m_type = m_nClientType;
  theHeader.m_cmd  = nCmdID;
  // 先填充名头头结构数据内容 => 注意是assign重建字符串...
  m_strSend.assign((char*)&theHeader, sizeof(theHeader));
  // 如果传入的数据内容有效，才进行数据的填充...
  if( lpJsonPtr != NULL && nJsonSize > 0 ) {
    m_strSend.append(lpJsonPtr, nJsonSize);
  }
  // 向当前终端对象发起发送数据事件...
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
