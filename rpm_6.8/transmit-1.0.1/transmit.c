#include <unistd.h>
#include <sys/types.h>      /* basic system data types */
#include <sys/socket.h>     /* basic socket definitions */
#include <netinet/tcp.h>    /* 2017.07.26 - by jackey */
#include <netinet/in.h>     /* sockaddr_in{} and other Internet defns */
#include <arpa/inet.h>      /* inet(3) functions */
#include <sys/epoll.h>      /* epoll function */
#include <fcntl.h>          /* nonblocking */
#include <sys/resource.h>   /*setrlimit */
#include <curl/curl.h>
#include <json/json.h>
#include <stdarg.h>
#include <assert.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <stdio.h>
#include <string>
#include <map>

#include "transmit.h"

using namespace std;

// STL must use g++...
// g++ -g transmit.c -o transmit -ljson
// valgrind --tool=memcheck --leak-check=full --show-reachable=yes ./transmit

#define SERVER_PORT           21001   // Transmit Port...
#define MAX_LINE          64 * 1024   // 2017.07.25 - by jackey => 避免录像任务命令，造成溢出...
#define MAX_LISTEN             1024   // 监听队列最大值...
#define MAX_EPOLL_SIZE         1024   // EPOLL队列最大值...
#define WAIT_TIME_OUT     10 * 1000   // 全局超时检测10秒...
#define SRS_TIME_OUT         5 * 60   // SRS服务器超时断开时间5分钟...
#define PLAY_TIME_OUT            30   // 播放器超时断开时间30秒...
#define LOG_MAX_SIZE           2048   // 单条日志最大长度...

void handleTimeout();
void clearAllServer();
void clearAllClient();
int  handleRead(int connfd);
int  handleWrite(int connfd);
int  setnonblocking(int sockfd);
int  gettcpstate(int sockfd);
int64_t buff2long(const char *buff);
void long2buff(int64_t n, char *buff);
const char * get_client_type(int inType);
const char * get_command_name(int inCmd);
int handleTransmitLiveVary(int nDBCameraID, int nUserCount);

// 定义日志处理函数和宏...
bool do_trace(const char * inFile, int inLine, const char *msg, ...);
#define log_trace(msg, ...) do_trace(__FILE__, __LINE__, msg, ##__VA_ARGS__)

class CClient;
class CPlayer;
class CCamera;
class CSrsServer;
typedef map<int, CPlayer*>          GM_MapPlayer; // PlayerID => CPlayer*
typedef map<int, int>               GM_MapLive;   // DBCameraID => user count
typedef map<int, CCamera*>          GM_MapCamera; // DBCameraID => CCamera*
typedef map<int, CClient*>          GM_MapConn;   // connfd   => CClient*
typedef map<string, string>         GM_MapJson;   // key      => value => JSON map object...
typedef map<string, CSrsServer*>    GM_MapServer; // string   => CSrsServer*

GM_MapServer  g_MapServer;                         // the global map object...
GM_MapConn	  g_MapConnect;                        // the global map object...

int g_kdpfd = 0;

class CPlayer
{
public:
  CPlayer(int nPlayerID, int nPlayerType);
  ~CPlayer();
public:
  void          SetPlayerType(int nType) { m_nPlayerType = nType; }
  int           GetPlayerType() { return m_nPlayerType; }
  int           GetPlayerID() { return m_nPlayerID; }
  void          ResetTimeout();
  bool          IsTimeout();
private:
  time_t        m_nStartTime;       // 超时检测起点 => HLS播放器才检测超时...
  int           m_nPlayerType;      // 播放器类型 => HTML5 or Flash...
  int           m_nPlayerID;        // 播放器编号 => 从1开始计数...
};

CPlayer::CPlayer(int nPlayerID, int nPlayerType)
  : m_nPlayerType(nPlayerType)
  , m_nPlayerID(nPlayerID)
{
  assert( m_nPlayerID > 0 );
  m_nStartTime = time(NULL);
}
//
// 重置超时计时器...
void CPlayer::ResetTimeout()
{
  m_nStartTime = time(NULL);  
}
//
// 判断是否已经超时 => 30秒没有汇报，就认为是超时...
// 备注：只有HTML5播放器才需要检测是否超时...
bool CPlayer::IsTimeout()
{
  if( m_nPlayerType == kFlash )
    return false;
  // 只有HTML5播放器才需要检测是否超时...
  time_t nDeltaTime = time(NULL) - m_nStartTime;
  return ((nDeltaTime >= PLAY_TIME_OUT) ? true : false);  
}

CPlayer::~CPlayer()
{
}

class CCamera
{
public:
  CCamera(int nDBCameraID);
  ~CCamera();
public:
  void    handlePlayerTimeout();
  void    DeleteAllFlashPlayer();
  int     AddNewPlayer(int nPlayerType = kHTML5);
  void    VerifyPlayer(int nPlayerID, int nPlayerType, bool bIsActive);
  int     GetPlayerCount() { return m_MapPlayer.size(); }
private:
  int           m_nMaxPlayerID;   // 该通道下当前最大播放器编号，计数器不断累加 => 通道被删除之后，重新计数，计数从1开始...
  int           m_nDBCameraID;    // 该通道的数据库编号...
  GM_MapPlayer  m_MapPlayer;      // 播放器列表，默认都是HTML5播放器...
};

CCamera::CCamera(int nDBCameraID)
  : m_nDBCameraID(nDBCameraID)
  , m_nMaxPlayerID(1)
{
}
//
// 删除所有的Flash播放器 => 当rtmp连接用户数为0时...
void CCamera::DeleteAllFlashPlayer()
{
  GM_MapPlayer::iterator itorItem = m_MapPlayer.begin();
  while( itorItem != m_MapPlayer.end() ) {
    // 如果播放器类型不是Flash，查找下一个...
    CPlayer * lpPlayer = itorItem->second;
    if( lpPlayer->GetPlayerType() != kFlash ) {
      ++itorItem; continue;
    }
    // 如果播放器是Flash，删除对象和算子...
    assert( lpPlayer->GetPlayerType() == kFlash );
    delete lpPlayer; lpPlayer = NULL;
    m_MapPlayer.erase(itorItem++);
  }
  // 打印调试信息...
  log_trace("Delete All Flash Player(Camera: %d, Player-Count: %d)", 
            m_nDBCameraID, m_MapPlayer.size());
}
//
// 当有播放器来获取播放地址时，会触发这个添加播放器的操作...
int CCamera::AddNewPlayer(int nPlayerType/* = kHTML5*/)
{
  // 播放器编号从0开始计数(包括0，这样利于编号回滚)...
  int nPlayerID = m_nMaxPlayerID++;
  CPlayer * lpPlayer = new CPlayer(nPlayerID, nPlayerType);
  // 将播放器对象保存到集合，并返回播放器编号...
  m_MapPlayer[nPlayerID] = lpPlayer;
  // 打印挂载播放器消息...
  log_trace("Create Player(ID: %d, Type: %s, Camera: %d, Player-Count: %d)", 
            nPlayerID, nPlayerType ? "HTML5" : "Flash", 
            m_nDBCameraID, m_MapPlayer.size());
  return nPlayerID;
}
//
// 处理HLS(HTML5)播放器每隔15秒的在线汇报通知...
void CCamera::VerifyPlayer(int nPlayerID, int nPlayerType, bool bIsActive)
{
  // 通过播放器编号查找到播放器对象，没有找到，直接返回..
  GM_MapPlayer::iterator itorItem = m_MapPlayer.find(nPlayerID);
  if( itorItem == m_MapPlayer.end() )
    return;
  CPlayer * lpPlayer = itorItem->second;
  assert( itorItem != m_MapPlayer.end() );
  // 找到播放器，但是，播放器发生错误，直接删除...
  if( !bIsActive ) {
    // 打印调试信息...
    log_trace("Delete Player(ID: %d, Type: %s, Camera: %d, Player-Count: %d)", 
              lpPlayer->GetPlayerID(), lpPlayer->GetPlayerType() ? "HTML5" : "Flash",
              m_nDBCameraID, m_MapPlayer.size() - 1);
    // 删除Player对象...
    delete lpPlayer; lpPlayer = NULL;
    m_MapPlayer.erase(itorItem);
    return;
  }
  // 如果播放器类型发生变化，打印信息...
  if( nPlayerType != lpPlayer->GetPlayerType() ) {
    log_trace("Change Player(ID: %d, Type: %s, Camera: %d, Player-Count: %d)", 
              lpPlayer->GetPlayerID(), nPlayerType ? "HTML5" : "Flash",
              m_nDBCameraID, m_MapPlayer.size());
  }
  // 播放器保持正常，直接修改播放器类型...
  assert(lpPlayer->GetPlayerID() == nPlayerID);
  lpPlayer->SetPlayerType(nPlayerType);
  // 直接重置超时...
  assert( bIsActive );
  lpPlayer->ResetTimeout();
}
//
// 检测通道下的播放器是否超时...
void CCamera::handlePlayerTimeout()
{
  GM_MapPlayer::iterator itorItem = m_MapPlayer.begin();
  while( itorItem != m_MapPlayer.end() ) {
    CPlayer * lpPlayer = itorItem->second;
    if( lpPlayer->IsTimeout() ) {
      // 打印调试信息...
      log_trace("Delete Player Timeout(ID: %d, Type: %s, Camera: %d, Player-Count: %d)", 
                lpPlayer->GetPlayerID(), lpPlayer->GetPlayerType() ? "HTML5" : "Flash",
                m_nDBCameraID, m_MapPlayer.size() - 1);
      // 发现播放器超时，直接删除，继续检测下一个...
      delete lpPlayer; lpPlayer = NULL;
      m_MapPlayer.erase(itorItem++);
    } else {
      // 播放器没有超时，继续检测下一个...
      ++itorItem;
    }
  }
}

CCamera::~CCamera()
{
  // 释放所有的播放器对象...
  GM_MapPlayer::iterator itorItem;
  for(itorItem = m_MapPlayer.begin(); itorItem != m_MapPlayer.end(); ++itorItem) {
    delete itorItem->second;
  }
  // 清空播放器集合...
  m_MapPlayer.clear();  
}

class CSrsServer
{
public:
  CSrsServer(string & strRtmpAddr, string & strHlsAddr);
  ~CSrsServer();
public:
  bool       IsTimeout();
  void       ResetTimeout();
  void       handlePlayerTimeout();
  CCamera *  doMountCamera(int nDBCameraID);
public:
  time_t        m_nStartTime;       // 超时检测起点...
  string        m_strRtmpAddr;      // 本直播服务器的rtmp地址 => IP:PORT
  string        m_strHlsAddr;       // 本直播服务器的hls地址  => IP:PORT
  GM_MapCamera  m_MapCamera;        // 本直播服务器已挂接的直播列表 => 有可能来自多个采集端...
};

CSrsServer::CSrsServer(string & strRtmpAddr, string & strHlsAddr)
  : m_strRtmpAddr(strRtmpAddr)
  , m_strHlsAddr(strHlsAddr)
{
  assert( m_strRtmpAddr.size() > 0 );
  assert( m_strHlsAddr.size() > 0 );
  m_nStartTime = time(NULL);
}

CSrsServer::~CSrsServer()
{
  // 释放所有的直播通道对象...
  GM_MapCamera::iterator itorItem;
  for(itorItem = m_MapCamera.begin(); itorItem != m_MapCamera.end(); ++itorItem) {
    delete itorItem->second;
  }
  // 清空通道集合...
  m_MapCamera.clear();  
}
//
// 重置超时计时器...
void CSrsServer::ResetTimeout()
{
  m_nStartTime = time(NULL);
}
//
// 判断是否已经超时 => 5分钟没有汇报，就认为是超时...
bool CSrsServer::IsTimeout()
{
  time_t nDeltaTime = time(NULL) - m_nStartTime;
  return ((nDeltaTime >= SRS_TIME_OUT) ? true : false);
}
//
// 将通道挂载并返回通道对象...
CCamera * CSrsServer::doMountCamera(int nDBCameraID)
{
  // 如果，通道已经挂载，直接返回挂载对象...
  GM_MapCamera::iterator itorItem = m_MapCamera.find(nDBCameraID);
  if( itorItem != m_MapCamera.end() )
    return itorItem->second;
  // 通道没有挂载，创建新通道，挂载上去...
  CCamera * lpCamera = new CCamera(nDBCameraID);
  m_MapCamera[nDBCameraID] = lpCamera;
  // 打印挂载通道的消息...
  log_trace("Create Camera(ID: %d, From: %s, Camera-Count: %d)", 
            nDBCameraID, m_strRtmpAddr.c_str(),
            m_MapCamera.size());
  return lpCamera;
}
//
// 遍历通道，检测通道下的播放器是否超时...
void CSrsServer::handlePlayerTimeout()
{
  GM_MapCamera::iterator itorItem = m_MapCamera.begin();
  while( itorItem != m_MapCamera.end() ) {
    // 遍历所有的通道，验证播放器是否超时...
    CCamera * lpCamera = itorItem->second;
    int nDBCameraID = itorItem->first;
    lpCamera->handlePlayerTimeout();
    // 如果该通道下的Flash+H5的用户数为0，需要通知采集端停止上传，删除该通道...
    if( lpCamera->GetPlayerCount() <= 0 ) {
      // 通知采集端停止上传...
      handleTransmitLiveVary(nDBCameraID, 0);
      // 删除当前通道...
      delete lpCamera; lpCamera = NULL;
      m_MapCamera.erase(itorItem++);
      // 打印删除通道的信息...
      log_trace("Delete Camera(ID: %d, From: %s, Camera-Count: %d)", 
                nDBCameraID, m_strRtmpAddr.c_str(),
                m_MapCamera.size());
    } else {
      // 通道有效，遍历下一个...
      ++itorItem;
    }
  }
}

class CClient
{
public:
  CClient(int connfd, int nSinPort, string & strSinAddr);
  ~CClient();
public:
  int       ForRead();          // 读取网络数据
  int       ForWrite();         // 发送网络数据
private:
  int       parseJsonData(const char * lpJsonPtr, int nJsonLength);          // 统一的JSON解析接口...
  int       doPHPGetCoreData(Cmd_Header * lpHeader);                         // 处理PHP获取核心数据事件...
  int       doPHPClient(Cmd_Header * lpHeader, const char * lpJsonPtr);      // 处理PHP客户端事件...
  int       doLiveClient(Cmd_Header * lpHeader, const char * lpJsonPtr);     // 处理Live服务器事件...
  int       doPlayClient(Cmd_Header * lpHeader, const char * lpJsonPtr);     // 处理Play播放器事件...
  int       doGatherClient(Cmd_Header * lpHeader, const char * lpJsonPtr);   // 处理采集端事件 => 长链接，需要检测...
  int       parseLiveJson(Cmd_Header * lpHeader, const char * lpJsonPtr);    // 解析直播服务器发送的JSON数据包...
  int       parsePlayJson(Cmd_Header * lpHeader, const char * lpJsonPtr);    // 解析播放终端发送的JSON数据包...
  int       doResponse(int nCmd, int nErrorCode, int nErrStatus = 0);
  int       doTransmitPlayLogin(int nPlayerSock, int nDBCameraID, char * lpRtmpUrl, int nUserCount);
  int       doTransmitLiveVary(int nDBCameraID, int nUserCount);
  int       doReturnPlayLogin(char * lpRtmpUrl, char * lpHlsUrl, int nPlayerID);
  
  CClient     * FindGatherByMAC(const char * lpMacAddr);
  CSrsServer  * FindSrsServer(int nDBCameraID);
  CCamera     * GetMountCamera(int nDBCameraID);
public:
  int         m_nConnFD;          // 连接socket...
  int         m_nSinPort;         // 连接端口...
  string      m_strSinAddr;       // 连接IP地址...
  int         m_nClientType;      // 客户端类型...
  string      m_strSend;          // 数据发送缓存...
  string      m_strRecv;          // 数据读取缓存...
  string      m_strMacGather;     // 记录本采集端的MAC地址...

  GM_MapJson  m_MapJson;          // 终端传递过来的JSON数据...
  GM_MapLive  m_MapLive;          // 记录本采集端挂接的直播列表...
};

CClient::CClient(int connfd, int nSinPort, string & strSinAddr)
  : m_nConnFD(connfd)
  , m_nSinPort(nSinPort)
  , m_strSinAddr(strSinAddr)
  , m_nClientType(kClientPHP)
{
  assert(m_nConnFD > 0 && m_strSinAddr.size() > 0 );
}

CClient::~CClient()
{
}
//
// 发送网络数据 => 始终设置读事件...
int CClient::ForWrite()
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
	if( epoll_ctl(g_kdpfd, EPOLL_CTL_MOD, m_nConnFD, &evClient) < 0 ) {
    log_trace("mod socket '%d' to epoll failed: %s", m_nConnFD, strerror(errno));
    return -1;
  }
  // 操作成功，返回0...
  return 0;
}
//
// 读取网络数据...
int CClient::ForRead()
{
  // 直接读取网络数据...
	char bufRead[MAX_LINE] = {0};
	int  nReadLen = read(m_nConnFD, bufRead, MAX_LINE);
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
    log_trace("Client Command(%s - %s, From: %s:%d, Socket: %d)", 
              get_client_type(m_nClientType), get_command_name(lpCmdHeader->m_cmd),
              this->m_strSinAddr.c_str(), this->m_nSinPort, this->m_nConnFD);
    // 对数据进行用户类型分发...
    switch( m_nClientType )
    {
      case kClientPHP:    nResult = this->doPHPClient(lpCmdHeader, lpDataPtr); break;
      case kClientLive:   nResult = this->doLiveClient(lpCmdHeader, lpDataPtr); break;
      case kClientPlay:   nResult = this->doPlayClient(lpCmdHeader, lpDataPtr); break;
      case kClientGather: nResult = this->doGatherClient(lpCmdHeader, lpDataPtr); break;
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
//
// 统一的JSON解析接口 => 保存到集合对象当中...
int CClient::parseJsonData(const char * lpJsonPtr, int nJsonLength)
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
// 处理PHP获取核心数据事件...
int CClient::doPHPGetCoreData(Cmd_Header * lpHeader)
{
  // 准备反馈需要的数据变量...
  TrackerHeader theTracker = {0};
  char szSendBuf[MAX_LINE] = {0};
  char szValue[255] = {0};
  // 组合反馈的json数据包...
  json_object * my_array = NULL;
  json_object * new_obj = json_object_new_object();
  json_object_object_add(new_obj, "err_code", json_object_new_int(ERR_OK));
  json_object_object_add(new_obj, "err_cmd", json_object_new_int(lpHeader->m_cmd));
  // 遍历所有已链接的客户端列表...
  if((lpHeader->m_cmd == kCmd_PHP_Get_All_Client) && (g_MapConnect.size() > 0)) {
    GM_MapConn::iterator itorConn;
    my_array = json_object_new_array();
    // 遍历构造数组内容...
    for(itorConn = g_MapConnect.begin(); itorConn != g_MapConnect.end(); ++itorConn) {
      CClient * lpClient = itorConn->second;
      sprintf(szValue, "%d:%s:%d", lpClient->m_nClientType, lpClient->m_strSinAddr.c_str(), lpClient->m_nSinPort);
      json_object_array_add(my_array, json_object_new_string(szValue));
    }
    // 将数组放入核心对象当中...
    json_object_object_add(new_obj, "err_data", my_array);
  }
  // 遍历所有已链接的直播服务器列表...
  if((lpHeader->m_cmd == kCmd_PHP_Get_Live_Server) && (g_MapServer.size() > 0)) {
    GM_MapServer::iterator itorServer;
    my_array = json_object_new_array();
    // 遍历构造数组内容...
    for(itorServer = g_MapServer.begin(); itorServer != g_MapServer.end(); ++itorServer) {
      string strRtmpAddr = itorServer->first;
      json_object_array_add(my_array, json_object_new_string(strRtmpAddr.c_str()));
    }
    // 将数组放入核心对象当中...
    json_object_object_add(new_obj, "err_data", my_array);
  }
  // 转换成json字符串，获取字符串长度...
  char * lpNewJson = (char*)json_object_to_json_string(new_obj);
  int nBodyLen = strlen(lpNewJson);
  // 组合TrackerHeader包头，方便php扩展使用 => 只需要设置pkg_len，其它置0...
  long2buff(nBodyLen, theTracker.pkg_len);
  memcpy(szSendBuf, &theTracker, sizeof(theTracker));
  memcpy(szSendBuf+sizeof(theTracker), lpNewJson, nBodyLen);
  // 将发送数据包缓存起来，等待发送事件到来...
  m_strSend.assign(szSendBuf, nBodyLen+sizeof(theTracker));
  // json对象引用计数减少...
  json_object_put(new_obj);
  // 准备修改事件需要的数据...
  struct epoll_event evClient = {0};
  evClient.data.fd = m_nConnFD;
  evClient.events = EPOLLOUT | EPOLLET;
  // 重新修改事件，加入写入事件...
  if( epoll_ctl(g_kdpfd, EPOLL_CTL_MOD, m_nConnFD, &evClient) < 0 ) {
    log_trace("mod socket '%d' to epoll failed: %s", m_nConnFD, strerror(errno));
    return -1;
  }
  // 返回0，等待写入事件...
  return 0;
}
//
// 处理PHP客户端事件...
int CClient::doPHPClient(Cmd_Header * lpHeader, const char * lpJsonPtr)
{
  // 处理特殊的获取中转服务器内部信息的命令...
  if( lpHeader->m_cmd == kCmd_PHP_Get_All_Client || lpHeader->m_cmd == kCmd_PHP_Get_Live_Server ) {
    return this->doPHPGetCoreData(lpHeader);
  }
  // 其它正常的跟采集端有关的命令...
  int nErrorCode = ERR_NO_MAC_ADDR;
  int nErrStatus = kGatherOffLine;
  // 检测输入参数MAC地址的有效性 => 没有找到MAC地址，直接返回错误...
  if( (m_MapJson.find("mac_addr") == m_MapJson.end()) || (m_MapJson["mac_addr"].size() <= 0) ) {
    return this->doResponse(lpHeader->m_cmd, nErrorCode, nErrStatus);
  }
  // 得到有效的当前MAC地址...
  string & strMacAddr = m_MapJson["mac_addr"];
  assert( strMacAddr.size() > 0 );
  // 这里为了转发给Gather，需要设置sock，便于Gather反馈之后的定位...
  lpHeader->m_sock = m_nConnFD;
  // 通过MAC地址查找采集端对象...
  nErrorCode = ERR_NO_GATHER;
  GM_MapConn::iterator itorConn;
  for(itorConn = g_MapConnect.begin(); itorConn != g_MapConnect.end(); ++itorConn) {
    CClient * lpClient = itorConn->second;
    if( lpClient->m_nClientType != kClientGather )
      continue;
    assert( lpClient->m_nClientType == kClientGather );
    if( strcasecmp(strMacAddr.c_str(), lpClient->m_strMacGather.c_str()) == 0 ) {
      // 如果是查询采集端状态，直接设置状态返回...
      if( lpHeader->m_cmd == kCmd_PHP_Get_Gather_Status ) {
        nErrStatus = kGatherOnLine;
        nErrorCode = ERR_OK;
        break;
      }
      // 找到了采集端，保存转发数据 => 发起发送事件...
      const char * lpBodyPtr = (const char *)lpHeader;
      int nBodySize = lpHeader->m_pkg_len + sizeof(Cmd_Header);
      lpClient->m_strSend.assign(lpBodyPtr, nBodySize);
      // 向采集端对象发起发送数据事件...
      struct epoll_event evClient = {0};
      evClient.data.fd = lpClient->m_nConnFD;
      evClient.events = EPOLLOUT | EPOLLET;
      // 重新修改事件，加入写入事件...
      if( epoll_ctl(g_kdpfd, EPOLL_CTL_MOD, lpClient->m_nConnFD, &evClient) < 0 ) {
        log_trace("mod socket '%d' to epoll failed: %s", lpClient->m_nConnFD, strerror(errno));
        nErrorCode = ERR_SOCK_SEND;
        break;
      }
      // 有些命令需要等待Gather反馈给PHP，因此，直接返回 => 这里不需要更改事件，仍然是EPOLLIN事件...
      // 2017.07.01 - by jackey => 这里只会发生 kCmd_PHP_Get_Course_Record 事件，kCmd_PHP_Get_Camera_Status不会被调用了...
      if( lpHeader->m_cmd == kCmd_PHP_Get_Camera_Status || lpHeader->m_cmd == kCmd_PHP_Get_Course_Record ) {
        return 0;
      }
      // 转发命令成功，设置错误号，反馈给PHP客服端...
      nErrorCode = ERR_OK;
      break;
    }
  }
  // 发生错误或其它Set命令，直接反馈结果...
  return this->doResponse(lpHeader->m_cmd, nErrorCode, nErrStatus);
}
//
// 通用的错误回复接口，返回统一的JSON数据包...
int CClient::doResponse(int nCmd, int nErrorCode, int nErrStatus/*= 0*/)
{
  // 将处理结果反馈给链接客户端...
  // 发生错误或其它Set命令，直接反馈结果，让直播服务器自己删除...
  TrackerHeader theTracker = {0};
  char szSendBuf[MAX_LINE] = {0};
  // 组合反馈的json数据包...
  json_object * new_obj = json_object_new_object();
  json_object_object_add(new_obj, "err_code", json_object_new_int(nErrorCode));
  json_object_object_add(new_obj, "err_cmd", json_object_new_int(nCmd));
  // 状态字段按照需要设置，而不是全都要设置...
  if( nCmd == kCmd_PHP_Get_Gather_Status ) {
    json_object_object_add(new_obj, "err_status", json_object_new_int(nErrStatus));
  }
  // 转换成json字符串，获取字符串长度...
  char * lpNewJson = (char*)json_object_to_json_string(new_obj);
  int nBodyLen = strlen(lpNewJson);
  // 组合TrackerHeader包头，方便php扩展使用 => 只需要设置pkg_len，其它置0...
  long2buff(nBodyLen, theTracker.pkg_len);
  memcpy(szSendBuf, &theTracker, sizeof(theTracker));
  memcpy(szSendBuf+sizeof(theTracker), lpNewJson, nBodyLen);

  // 将发送数据包缓存起来，等待发送事件到来...
  m_strSend.assign(szSendBuf, nBodyLen+sizeof(theTracker));
  // json对象引用计数减少...
  json_object_put(new_obj);
  // 准备修改事件需要的数据...
  struct epoll_event evClient = {0};
  evClient.data.fd = m_nConnFD;
  evClient.events = EPOLLOUT | EPOLLET;
  // 重新修改事件，加入写入事件...
  if( epoll_ctl(g_kdpfd, EPOLL_CTL_MOD, m_nConnFD, &evClient) < 0 ) {
    log_trace("mod socket '%d' to epoll failed: %s", m_nConnFD, strerror(errno));
    return -1;
  }
  // 返回0，等待写入事件...
  return 0;
}
//
// 处理Live服务器事件 => 直播一定要调用 doResponse...
int CClient::doLiveClient(Cmd_Header * lpHeader, const char * lpJsonPtr)
{
  // 解析直播服务器发送的JSON数据包，并建立集合存放数据...
  int nErrorCode = this->parseLiveJson(lpHeader, lpJsonPtr);
  return this->doResponse(lpHeader->m_cmd, nErrorCode);
}
//
// 解析直播服务器发送的JSON数据包...
int CClient::parseLiveJson(Cmd_Header * lpHeader, const char * lpJsonPtr)
{
  // 判断解析出来的数据对象是否有效...
  if( m_MapJson.find("rtmp_addr") == m_MapJson.end() ||
    m_MapJson.find("rtmp_live") == m_MapJson.end() ||
    m_MapJson.find("rtmp_user") == m_MapJson.end() ||
    m_MapJson.find("hls_addr") == m_MapJson.end() ) {
    return ERR_NO_JSON;  
  }
  // rtmp_live => 数据库当中的摄像头编号(DBCameraID)
  // rtmp_user => 这个摄像头下面的用户数量
  // rtmp_addr => 直播端地址  => IP:PORT
  // hls_addr  => HLS播放地址 => IP:PORT
  string & strHlsAddr = m_MapJson["hls_addr"];
  string & strRtmpAddr = m_MapJson["rtmp_addr"];
  int nRtmpLive = atoi(m_MapJson["rtmp_live"].c_str());
  int nRtmpUser = atoi(m_MapJson["rtmp_user"].c_str());
  // 创建直播对象，并存放到集合当中...
  CSrsServer * lpServer = NULL;
  GM_MapServer::iterator itorItem;
  itorItem = g_MapServer.find(strRtmpAddr);
  // 首先，处理直播服务器 quit 命令...
  if( lpHeader->m_cmd == kCmd_Live_Quit ) {
    // 没有找到相关节点，直接返回...
    if( itorItem == g_MapServer.end() )
      return ERR_OK;
    // 找到相关节点，直接删除...
    lpServer = itorItem->second;
    log_trace("Delete SRS, RTMP: %s, SRS-Count: %d", 
              lpServer->m_strRtmpAddr.c_str(), 
              g_MapServer.size() - 1);
    // 删除直播服务器，包括节点算子...
    delete lpServer; lpServer = NULL;
    g_MapServer.erase(itorItem);
    return ERR_OK;
  }
  // 处理 login(直播服务器) 和 vary(通道) 命令...
  if( itorItem != g_MapServer.end() ) {
    lpServer = itorItem->second;
  } else {
    lpServer = new CSrsServer(strRtmpAddr, strHlsAddr);
    g_MapServer[strRtmpAddr] = lpServer;
    // 打印创建直播服务器消息...
    log_trace("Create SRS, RTMP: %s, SRS-Count: %d", 
              lpServer->m_strRtmpAddr.c_str(), 
              g_MapServer.size());
  }
  // 重置超时计时器...
  lpServer->ResetTimeout();
  // 直播服务器 login 命令，不转发给采集端...
  if( lpHeader->m_cmd == kCmd_Live_Login )
    return ERR_OK;
  // 只有 vary 命令才转发给采集端 => 通道上的flash用户为0了...
  assert( nRtmpLive > 0 && nRtmpUser >= 0 );
  assert( lpHeader->m_cmd == kCmd_Live_Vary );
  // 需要找到挂载的通道对象...
  CCamera * lpCamera = this->GetMountCamera(nRtmpLive);
  if( lpCamera == NULL )
    return ERR_OK;
  // 删除该通道下面所有的flash播放器...
  lpCamera->DeleteAllFlashPlayer();
  // 获取通道上有效的用户数 => Flash用户 + HTML5用户
  int nUserCount = lpCamera->GetPlayerCount();
  // 当用户总数为0时，需要通知对应的采集端删除直播频道，停止上传；不为0，则直接返回...
  return this->doTransmitLiveVary(nRtmpLive, nUserCount);
}
//
// 通知相关的采集端，删除指定的通道编号上传...
int CClient::doTransmitLiveVary(int nDBCameraID, int nUserCount)
{
  return handleTransmitLiveVary(nDBCameraID, nUserCount);
}
//
// 对转发通知，进行了全局封装...
int handleTransmitLiveVary(int nDBCameraID, int nUserCount)
{
  // 需要找到挂接了指定通道的采集端对象...
  CClient * lpGather = NULL;
  GM_MapConn::iterator itorConn;
  for(itorConn = g_MapConnect.begin(); itorConn != g_MapConnect.end(); ++itorConn) {
    CClient * lpClient = itorConn->second;
    if( lpClient->m_nClientType != kClientGather )
      continue;
    // 查找采集端下面对应的通道集合，是否已经挂接了指定通道...
    assert( lpClient->m_nClientType == kClientGather );
    GM_MapLive & theMapLive = lpClient->m_MapLive;
    if( theMapLive.find(nDBCameraID) != theMapLive.end() ) {
      lpGather = lpClient;
      break;
    }
  }
  // 如果没有挂接指定通道，直接返回错误号...
  if( lpGather == NULL )
    return ERR_NO_GATHER;
  assert( lpGather != NULL );
  // 找到了采集端，先对通道下的用户数进行更新操作...
  lpGather->m_MapLive[nDBCameraID] = nUserCount;
  // 如果该通道上的用户数 > 0，不转发命令，直接返回...
  if( nUserCount > 0 )
    return ERR_OK;
  assert( nUserCount <= 0 );
  // 如果该通道上的用户数 <= 0 ，需要通知对应的采集端删除直播频道，停止上传...
  // 构造转发JSON数据块...
  char szSendBuf[MAX_LINE] = {0};
  json_object * new_obj = json_object_new_object();
  json_object_object_add(new_obj, "rtmp_live", json_object_new_int(nDBCameraID));
  json_object_object_add(new_obj, "rtmp_user", json_object_new_int(nUserCount));
  // 转换成json字符串，获取字符串长度...
  char * lpNewJson = (char*)json_object_to_json_string(new_obj);
  int nBodyLen = strlen(lpNewJson);
  // 构造转发结构头信息...
  Cmd_Header theHeader = {0};
  theHeader.m_sock = 0;
  theHeader.m_type = kClientLive;
  theHeader.m_cmd  = kCmd_Live_Vary;
  theHeader.m_pkg_len = nBodyLen;
  memcpy(szSendBuf, &theHeader, sizeof(theHeader));
  memcpy(szSendBuf+sizeof(theHeader), lpNewJson, nBodyLen);
  // 向采集端对象发送组合后的数据包...
  assert( lpGather->m_nClientType == kClientGather );
  lpGather->m_strSend.assign(szSendBuf, nBodyLen+sizeof(theHeader));
  // json对象引用计数减少...
  json_object_put(new_obj);
  // 向采集端对象发起发送数据事件...
  struct epoll_event evClient = {0};
  evClient.data.fd = lpGather->m_nConnFD;
  evClient.events = EPOLLOUT | EPOLLET;
  // 重新修改事件，加入写入事件...
  if( epoll_ctl(g_kdpfd, EPOLL_CTL_MOD, lpGather->m_nConnFD, &evClient) < 0 ) {
    log_trace("mod socket '%d' to epoll failed: %s", lpGather->m_nConnFD, strerror(errno));
    return ERR_SOCK_SEND;
  }
  // 发送成功，反馈正确结果...
  return ERR_OK;
}
//
// 处理Play播放器事件...
int CClient::doPlayClient(Cmd_Header * lpHeader, const char * lpJsonPtr)
{
  // 解析播放终端发送的JSON数据包...
  int nErrorCode = this->parsePlayJson(lpHeader, lpJsonPtr);
  // 如果没有错误，直接返回，已经构造了数据直接返回给了播放器，不必等待采集端回应...
  if( nErrorCode <= ERR_OK )
    return nErrorCode;
  // 如果有错误，直接反馈执行结果，不要通知采集端...
  assert( nErrorCode > ERR_OK );
  return this->doResponse(lpHeader->m_cmd, nErrorCode);
}
//
// 解析播放终端发送的JSON数据包...
int CClient::parsePlayJson(Cmd_Header * lpHeader, const char * lpJsonPtr)
{
  // 判断解析出来的数据对象是否有效，mac地址为空也返回错误...
  if( m_MapJson.find("rtmp_live") == m_MapJson.end() )
    return ERR_NO_JSON;
  // rtmp_live => 数据库当中的摄像头编号(DBCameraID)
  int nRtmpLive = atoi(m_MapJson["rtmp_live"].c_str());
  // 进行命令分发和判断，检查通道和命令的有效性...
  if( nRtmpLive <= 0 )
    return ERR_NO_COMMAND;
  // 处理播放器登录命令...
  if( lpHeader->m_cmd == kCmd_Play_Login ) {
    // 必须包含mac_addr，才能找到对应的采集端...
    if( m_MapJson.find("mac_addr") == m_MapJson.end() || 
      m_MapJson["mac_addr"].size() <= 0 ) {
      return ERR_NO_JSON;
    }
    // mac_addr  => 采集端MAC地址，用户查询采集端...
    string & strMacAddr = m_MapJson["mac_addr"];
    // 首先，根据MAC地址查找采集端，没有采集端...
    CClient * lpGather = this->FindGatherByMAC(strMacAddr.c_str());
    if( lpGather == NULL )
      return ERR_NO_GATHER;
    assert( lpGather != NULL );
    // 通过请求的直播通道编号查找直播服务器...
    CSrsServer * lpServer = this->FindSrsServer(nRtmpLive);
    if( lpServer == NULL )
      return ERR_NO_RTMP;
    // 将当前通道挂载到选定的服务器当中...
    char szRtmpURL[256] = {0};
    string & strRtmpAddr = lpServer->m_strRtmpAddr;
    CCamera * lpCamera = lpServer->doMountCamera(nRtmpLive);
    sprintf(szRtmpURL, "rtmp://%s/live/live%d", strRtmpAddr.c_str(), nRtmpLive);
    // 在通道对象上，创建一个新的播放器对象，并返回这个播放器编号...
    int nPlayerID = lpCamera->AddNewPlayer();
    int nPlayerCount = lpCamera->GetPlayerCount();
    // 转发播放器登录命令到采集端...
    assert( this->m_nClientType == kClientPlay );
    int nErrorCode = lpGather->doTransmitPlayLogin(m_nConnFD, nRtmpLive, szRtmpURL, nPlayerCount);
    if( nErrorCode != ERR_OK )
      return nErrorCode;
    // 转发命令发送成功，直接返回给播放器 rtmp/hls 地址，无需等待采集端的转发结果...
    char szHlsURL[256] = {0};
    string & strHlsAddr = lpServer->m_strHlsAddr;
    sprintf(szHlsURL, "http://%s/live/live%d.m3u8", strHlsAddr.c_str(), nRtmpLive);
    return this->doReturnPlayLogin(szRtmpURL, szHlsURL, nPlayerID);
  } else if( lpHeader->m_cmd == kCmd_Play_Verify ) {
    // 处理播放器汇报命令，只有HTML5播放器才会每隔12秒汇报，flash播放器只汇报一次...
    // 必须包含player_id编号数据...
    if( m_MapJson.find("player_id") == m_MapJson.end() ||
        m_MapJson.find("player_type") == m_MapJson.end() ||
        m_MapJson.find("player_active") == m_MapJson.end() )
      return ERR_NO_JSON;
    // 获取解析到的数据...
    int  nPlayerID = atoi(m_MapJson["player_id"].c_str());
    int  nPlayerType = atoi(m_MapJson["player_type"].c_str());
    bool bIsActive = atoi(m_MapJson["player_active"].c_str());
    // 找到当前通道挂接的服务器对象...
    CCamera * lpCamera = this->GetMountCamera(nRtmpLive);
    if( lpCamera == NULL )
      return ERR_NO_RTMP;
    // 在这个通道上执行汇报命令...
    lpCamera->VerifyPlayer(nPlayerID, nPlayerType, bIsActive);
    // 直接返回，让PHP扩展停止等待...
    return this->doResponse(lpHeader->m_cmd, ERR_OK);
  }
  // 如果是其它未知的命令，直接返回错误...
  return ERR_NO_COMMAND;
}
//
// 转发播放器登录命令到采集端 => 这里是采集端对象...
int CClient::doTransmitPlayLogin(int nPlayerSock, int nDBCameraID, char * lpRtmpUrl, int nUserCount)
{
  // 判断输入数据的有效性...
  if( nPlayerSock <= 0 || nDBCameraID <= 0 || lpRtmpUrl == NULL )
    return ERR_NO_JSON;
  // 在集合中保存通道信息，证明当前采集点挂载了这个通道...
  m_MapLive[nDBCameraID] = nUserCount;
  // 构造转发JSON数据块...
  char szSendBuf[MAX_LINE] = {0};
  json_object * new_obj = json_object_new_object();
  json_object_object_add(new_obj, "rtmp_url", json_object_new_string(lpRtmpUrl));
  json_object_object_add(new_obj, "rtmp_live", json_object_new_int(nDBCameraID));
  json_object_object_add(new_obj, "rtmp_user", json_object_new_int(nUserCount));
  // 转换成json字符串，获取字符串长度...
  char * lpNewJson = (char*)json_object_to_json_string(new_obj);
  int nBodyLen = strlen(lpNewJson);
  // 构造转发结构头信息...
  Cmd_Header theHeader = {0};
  theHeader.m_type = kClientPlay;
  theHeader.m_sock = nPlayerSock;
  theHeader.m_cmd  = kCmd_Play_Login;
  theHeader.m_pkg_len = nBodyLen;
  memcpy(szSendBuf, &theHeader, sizeof(theHeader));
  memcpy(szSendBuf+sizeof(theHeader), lpNewJson, nBodyLen);
  // 向采集端对象发送组合后的数据包...
  assert( m_nClientType == kClientGather );
  m_strSend.assign(szSendBuf, nBodyLen+sizeof(theHeader));
  // json对象引用计数减少...
  json_object_put(new_obj);
  // 向采集端对象发起发送数据事件...
  struct epoll_event evClient = {0};
  evClient.data.fd = m_nConnFD;
  evClient.events = EPOLLOUT | EPOLLET;
  // 重新修改事件，加入写入事件...
  if( epoll_ctl(g_kdpfd, EPOLL_CTL_MOD, m_nConnFD, &evClient) < 0 ) {
    log_trace("mod socket '%d' to epoll failed: %s", m_nConnFD, strerror(errno));
    return ERR_SOCK_SEND;
  }
  // 返回执行正确...
  return ERR_OK;
}
//
// 直接反馈直播地址给播放器，无需等待采集端的反馈，避免php阻塞 => 这里是播放器对象...
int CClient::doReturnPlayLogin(char * lpRtmpUrl, char * lpHlsUrl, int nPlayerID)
{
  // 这里需要特别注意 => 转发给PHP客服端的数据都需要加上TrackerHeader，便于PHP扩展接收数据...
  TrackerHeader theTracker = {0};
  char szSendBuf[MAX_LINE] = {0};
  // 组合反馈的json数据包...
  json_object * new_obj = json_object_new_object();
  json_object_object_add(new_obj, "player_id", json_object_new_int(nPlayerID));
  json_object_object_add(new_obj, "rtmp_url", json_object_new_string(lpRtmpUrl));
  json_object_object_add(new_obj, "rtmp_type", json_object_new_string("rtmp/flv"));
  json_object_object_add(new_obj, "hls_url", json_object_new_string(lpHlsUrl));
  json_object_object_add(new_obj, "hls_type", json_object_new_string("application/x-mpegURL"));
  // 转换成json字符串，获取字符串长度...
  char * lpNewJson = (char*)json_object_to_json_string(new_obj);
  int nBodyLen = strlen(lpNewJson);
  // 组合TrackerHeader包头，方便php扩展使用 => 只需要设置pkg_len，其它置0...
  long2buff(nBodyLen, theTracker.pkg_len);
  memcpy(szSendBuf, &theTracker, sizeof(theTracker));
  memcpy(szSendBuf+sizeof(theTracker), lpNewJson, nBodyLen);
  // 将发送数据包缓存起来，等待发送事件到来...
  assert( m_nClientType == kClientPlay );
  m_strSend.assign(szSendBuf, nBodyLen+sizeof(theTracker));
  // json对象引用计数减少...
  json_object_put(new_obj);
  // 准备修改事件需要的数据...
  struct epoll_event evClient = {0};
  evClient.data.fd = m_nConnFD;
  evClient.events = EPOLLOUT | EPOLLET;
  // 重新修改事件，加入写入事件...
  if( epoll_ctl(g_kdpfd, EPOLL_CTL_MOD, m_nConnFD, &evClient) < 0 ) {
    log_trace("mod socket '%d' to epoll failed: %s", m_nConnFD, strerror(errno));
    return -1;
  }
  // 返回0，等待写入事件...
  return ERR_OK;
}
//
// 根据MAC地址查找采集端...
CClient * CClient::FindGatherByMAC(const char * lpMacAddr)
{
  // 传入地址为空，返回空值...
  if( lpMacAddr == NULL || strlen(lpMacAddr) <= 0 )
    return NULL;
  GM_MapConn::iterator itorConn;
  for(itorConn = g_MapConnect.begin(); itorConn != g_MapConnect.end(); ++itorConn) {
    CClient * lpClient = itorConn->second;
    // 客户端必须是采集端，并且mac地址是一致的...
    if( (lpClient->m_nClientType == kClientGather) && (strcasecmp(lpMacAddr, lpClient->m_strMacGather.c_str()) == 0) ) {
      return lpClient;
    }
  }
  return NULL;
}
//
// 通过请求的直播通道编号查找直播服务器...
CSrsServer * CClient::FindSrsServer(int nDBCameraID)
{
  // 如果没有直播汇报源，直接返回NULL...
  if( g_MapServer.size() <= 0 )
    return NULL;
  assert( g_MapServer.size() > 0 );
  // 先拿出一个节点的数据作为最小值...
  GM_MapServer::iterator itorItem = g_MapServer.begin();
  CSrsServer * lpMinServer = itorItem->second;
  int nMinCount = lpMinServer->m_MapCamera.size();
  // 遍历所有节点，找到最小的节点...
  for(itorItem = g_MapServer.begin(); itorItem != g_MapServer.end(); ++itorItem) {
    CSrsServer * lpCurServer = itorItem->second;
    GM_MapCamera & theMapCamera = lpCurServer->m_MapCamera;
    // 计算最小挂载量的直播服务器...
    if( theMapCamera.size() < nMinCount ) {
      nMinCount = theMapCamera.size();
      lpMinServer = lpCurServer;
    }
    // 如果指定的通道已经挂载到了当前直播服务器上，直接返回这个服务器..
    if( theMapCamera.find(nDBCameraID) != theMapCamera.end() )
      return lpCurServer;
  }
  // 直接返回最小挂在量的直播服务器对象...
  assert( lpMinServer != NULL );
  return lpMinServer;
}
//
// 找到挂接通道的通道对象...
CCamera * CClient::GetMountCamera(int nDBCameraID)
{
  // 如果没有直播汇报源，直接返回NULL...
  if( g_MapServer.size() <= 0 )
    return NULL;
  assert( g_MapServer.size() > 0 );
  GM_MapServer::iterator itorItem = g_MapServer.begin();
  while( itorItem != g_MapServer.end() ) {
    // 遍历所有的直播服务器...
    CSrsServer * lpServer = itorItem->second;
    GM_MapCamera & theMapCamera = lpServer->m_MapCamera;
    GM_MapCamera::iterator itorCamera = theMapCamera.find(nDBCameraID);
    // 当前通道列表中，找到了通道，直接返回这个通道对象...
    if( itorCamera != theMapCamera.end() )
      return itorCamera->second;
    // 没有找到通道，继续查找...
    ++itorItem;
  }
  // 最终，没有找到挂载的服务器，直接返回空...
  return NULL;
}
//
// 处理采集端事件...
int CClient::doGatherClient(Cmd_Header * lpHeader, const char * lpJsonPtr)
{
  if( lpHeader->m_cmd == kCmd_Gather_Login ) {
    // 处理采集端登录过程 => 判断传递JSON数据有效性...
    if( m_MapJson.find("mac_addr") == m_MapJson.end() ||
      m_MapJson.find("ip_addr") == m_MapJson.end() ||
      m_MapJson["mac_addr"].size() <= 0 ) {
      return -1;
    }
    // 保存解析到的有效JSON数据项...
    m_strMacGather = m_MapJson["mac_addr"];
    m_strSinAddr = m_MapJson["ip_addr"];
  } else {
    // 2017.06.15 - by jackey => 取消了采集端延时转发给播放器的命令，避免php阻塞...
    // 2017.07.01 - by jackey => php客户端的 kCmd_PHP_Get_Course_Record 命令依然需要中转...
    // 判断PHP客户端是否有效，没有找到，记录，直接返回...
    CClient * lpClient = NULL;
    GM_MapConn::iterator itorPHP = g_MapConnect.find(lpHeader->m_sock);
    if( itorPHP == g_MapConnect.end() ) {
      log_trace("php client closed!");
      return 0;
    }
    // 获得需要反馈的客户端对象 => 只可能是php客户端的 kCmd_PHP_Get_Course_Record 命令...
    lpClient = itorPHP->second; assert( lpClient != NULL );
    assert( lpClient->m_nConnFD == lpHeader->m_sock );
    // 得到Gather反馈回来的json数据包 => 有效性已经在前面验证过了...
    // 这里需要特别注意 => 转发给PHP客服端的数据都需要加上TrackerHeader，便于PHP扩展接收数据...
    char szSendBuf[MAX_LINE] = {0};
    TrackerHeader theTracker = {0};
    // 获取JSON数据包长度...
    int nBodyLen = lpHeader->m_pkg_len;
    assert( nBodyLen == strlen(lpJsonPtr) );
    // 组合TrackerHeader包头，方便php扩展使用 => 只需要设置pkg_len，其它置0...
    long2buff(nBodyLen, theTracker.pkg_len);
    memcpy(szSendBuf, &theTracker, sizeof(theTracker));
    memcpy(szSendBuf+sizeof(theTracker), lpJsonPtr, nBodyLen);
    // 向PHP对象发送组合后的数据包...
    lpClient->m_strSend.assign(szSendBuf, nBodyLen+sizeof(theTracker));
    // 向播放器对象发起发送数据事件...
    struct epoll_event evClient = {0};
    evClient.data.fd = lpClient->m_nConnFD;
    evClient.events = EPOLLOUT | EPOLLET;
    // 重新修改事件，加入写入事件...
    if( epoll_ctl(g_kdpfd, EPOLL_CTL_MOD, lpClient->m_nConnFD, &evClient) < 0 ) {
      log_trace("mod socket '%d' to epoll failed: %s", lpClient->m_nConnFD, strerror(errno));
      return 0;
    }
  }
  return 0;
}

char g_absolute_path[256] = {0};
int main(int argc, char **argv)
{
  // find the current path...
  int cnt = readlink("/proc/self/exe", g_absolute_path, 256);
  if( cnt < 0 || cnt >= 256 ) {
    fprintf(stderr, "readlink error: %s", strerror(errno));
    return -1;
  }
  for( int i = cnt; i >= 0; --i ) {
    if( g_absolute_path[i] == '/' ) {
      g_absolute_path[i+1] = '\0';
      break;
    }
  }
  // append the slash flag...
  if( g_absolute_path[strlen(g_absolute_path) - 1] != '/' ) {  
    g_absolute_path[strlen(g_absolute_path)] = '/';  
  }
  // create transmit.log file...
  strcat(g_absolute_path, "transmit.log");
  
	// set max open file number for one process...
	struct rlimit rt = {0};
	rt.rlim_max = rt.rlim_cur = MAX_EPOLL_SIZE;
	if( setrlimit(RLIMIT_NOFILE, &rt) == -1 ) {
    log_trace("setrlimit error(%s)", strerror(errno));
		return -1;
	}
  // create server address...
	struct sockaddr_in servaddr = {0};
	bzero(&servaddr, sizeof(servaddr));
	servaddr.sin_family = AF_INET; 
	servaddr.sin_addr.s_addr = htonl (INADDR_ANY);
	servaddr.sin_port = htons(SERVER_PORT);
  // create server socket...
	int listenfd = socket(AF_INET, SOCK_STREAM, 0); 
	if( listenfd == -1 ) {
    log_trace("can't create socket");
		return -1;
	}
  // server socket reuse address...
	int opt = 1;
	setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
  // server socket nonblocking => error not close...
	if( setnonblocking(listenfd) < 0 ) {
    log_trace("setnonblock error");
	}
  // bind server listen port...
	if( bind(listenfd, (struct sockaddr *)&servaddr, sizeof(struct sockaddr)) == -1 ) {
    log_trace("bind error");
		return -1;
	}
  // start listen queue...
	if( listen(listenfd, MAX_LISTEN) == -1 ) {
    log_trace("listen error");
		return -1;
	}
  // create epoll handle, add socket to epoll events...
  struct epoll_event g_events[MAX_EPOLL_SIZE] = {0};
  struct epoll_event evListen = {0};
	int curfds = 1, acceptCount = 0;
	g_kdpfd = epoll_create(MAX_EPOLL_SIZE);
	evListen.data.fd = listenfd;
	evListen.events = EPOLLIN | EPOLLET; // EPOLLEF模式下，accept时必须用循环来接收链接，防止链接丢失...
	if( epoll_ctl(g_kdpfd, EPOLL_CTL_ADD, listenfd, &evListen) < 0 ) {
    log_trace("epoll set insertion error: fd=%d", listenfd);
		return -1;
	}
  // print startup log...
  log_trace("transmit-server startup, port %d, max-connection is %d, backlog is %d", SERVER_PORT, MAX_EPOLL_SIZE, MAX_LISTEN);
  // begin the epoll event...
  while( true ) {
		// wait for epoll event...
		int nfds = epoll_wait(g_kdpfd, g_events, curfds, WAIT_TIME_OUT);
    // when error == EINTR, continue...
		if( nfds == -1 ) {
      log_trace("epoll_wait error(code:%d, %s)", errno, strerror(errno));
      // is EINTR, continue...
      if( errno == EINTR ) 
        continue;
      // not EINTR, break...
      assert( errno != EINTR );
      break;
		}
    // 处理超时的情况 => 释放一些已经死掉的资源...
    if( nfds == 0 ) {
      handleTimeout();
      continue;
    }
    // 处理正确返回值的情况...
    for(int n = 0; n < nfds; ++n) {
      // 处理服务器socket事件...
      int nCurEventFD = g_events[n].data.fd;
			if( nCurEventFD == listenfd ) {
        // 这里要循环accept链接，可能会有多个链接同时到达...
        while( true ) {
          // 收到客户端连接的socket...
          struct sockaddr_in cliaddr = {0};
          socklen_t socklen = sizeof(struct sockaddr_in);
          int connfd = accept(listenfd, (struct sockaddr *)&cliaddr, &socklen);
          // 发生错误，直接记录，继续...
          if( connfd < 0 ) {
            //log_trace("accept error(%d)", errno);
            break;
          }
          // eqoll队列超过最大值，关闭，继续...
          if( curfds >= MAX_EPOLL_SIZE ) {
            log_trace("too many connection, more than %d", MAX_EPOLL_SIZE);
            close(connfd);
            break; 
          }
          // set none blocking for the new client socket => error not close... 
          if( setnonblocking(connfd) < 0 ) {
            log_trace("setnonblocking error");
          }
          // 添加新socket到epoll事件池 => 只加入读取事件...
          struct epoll_event evClient = {0};
          evClient.events = EPOLLIN | EPOLLET;
          evClient.data.fd = connfd;
          // 添加失败，记录，继续...
          if( epoll_ctl(g_kdpfd, EPOLL_CTL_ADD, connfd, &evClient) < 0 ) {
            log_trace("add socket '%d' to epoll failed: %s", connfd, strerror(errno));
            close(connfd);
            break;
          }
          // 全部成功，打印信息，引用计数增加...
          ++curfds; ++acceptCount;
          int nSinPort = cliaddr.sin_port;
          string strSinAddr = inet_ntoa(cliaddr.sin_addr);
          //log_trace("client count(%d) - increase, accept from %s:%d", acceptCount, strSinAddr.c_str(), nSinPort);
          // 创建客户端对象,并保存到集合当中...
          CClient * lpClient = new CClient(connfd, nSinPort, strSinAddr);
          g_MapConnect[connfd] = lpClient;
        }
      } else {
        // 处理客户端socket事件...
        int nRetValue = -1;
        if( g_events[n].events & EPOLLIN ) {
          nRetValue = handleRead(nCurEventFD);
        } else if( g_events[n].events & EPOLLOUT ) {
          nRetValue = handleWrite(nCurEventFD);
        }
        // 判断处理结果...
        if( nRetValue < 0 ) {
          // 处理失败，从epoll队列中删除...
          struct epoll_event evDelete = {0};
          evDelete.data.fd = nCurEventFD;
          evDelete.events = EPOLLIN | EPOLLET;
          epoll_ctl(g_kdpfd, EPOLL_CTL_DEL, nCurEventFD, &evDelete);
          // 删除对应的客户端连接对象...
          if( g_MapConnect.find(nCurEventFD) != g_MapConnect.end() ) {
            delete g_MapConnect[nCurEventFD];
            g_MapConnect.erase(nCurEventFD);
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
  clearAllClient();
  // clear all the live server...
  clearAllServer();
  // close listen socket and exit...
  log_trace("transmit-server exit.");
  close(listenfd);
  return 0; 
}
//
// 处理超时的情况...
void handleTimeout()
{
  // 2017.07.26 - by jackey => 根据连接状态删除客户端...
  // 遍历所有的连接，判断是否有效，无效直接删除...
  CClient * lpClient = NULL;
  GM_MapConn::iterator itorConn;
  itorConn = g_MapConnect.begin();
  while( itorConn != g_MapConnect.end() ) {
    lpClient = itorConn->second;
    if( !gettcpstate(itorConn->first) ) {
      log_trace("client type(%d) be killed by handleTimeout()", lpClient->m_nClientType);
      delete lpClient; lpClient = NULL;
      g_MapConnect.erase(itorConn++);
    } else {
      ++itorConn;
    }
  }
  // 遍历所有的直播服务器，判断是否发生了超时...
  GM_MapServer::iterator itorItem;
  CSrsServer * lpServer = NULL;
  itorItem = g_MapServer.begin();
  while( itorItem != g_MapServer.end() ) {
    lpServer = itorItem->second;
    assert( lpServer != NULL );
    if( lpServer->IsTimeout() ) {
      delete lpServer; lpServer = NULL;
      g_MapServer.erase(itorItem++);
    } else {
      // 服务器未超时，检测挂载的播放器是否超时...
      lpServer->handlePlayerTimeout();
      ++itorItem;
    }
  }
}
//
// 处理客户端socket读取事件...
int handleRead(int connfd)
{
  // 在集合中查找客户端对象...
  GM_MapConn::iterator itorConn = g_MapConnect.find(connfd);
  if( itorConn == g_MapConnect.end() ) {
    log_trace("can't find client connection(%d)", connfd);
		return -1;
  }
  // 获取对应的客户端对象，执行读取操作...
  CClient * lpClient = itorConn->second;
  return lpClient->ForRead();
}
//
// 处理客户端socket写入事件...
int handleWrite(int connfd)
{
  // 在集合中查找客户端对象...
  GM_MapConn::iterator itorConn = g_MapConnect.find(connfd);
  if( itorConn == g_MapConnect.end() ) {
    log_trace("can't find client connection(%d)", connfd);
		return -1;
  }
  // 获取对应的客户端对象，执行写入操作...
  CClient * lpClient = itorConn->second;
  return lpClient->ForWrite();
}
//
// 删除所有的直播服务器连接...
void clearAllServer()
{
  GM_MapServer::iterator itorItem;
  for(itorItem = g_MapServer.begin(); itorItem != g_MapServer.end(); ++itorItem) {
    delete itorItem->second;
  }
  g_MapServer.clear();
}
//
// 删除所有的客户端连接...
void clearAllClient()
{
  GM_MapConn::iterator itorItem;
  for(itorItem = g_MapConnect.begin(); itorItem != g_MapConnect.end(); ++itorItem) {
    delete itorItem->second;
  }
  g_MapConnect.clear();
}
//
// set non-block for socket...
int setnonblocking(int sockfd)
{
	if (fcntl(sockfd, F_SETFL, fcntl(sockfd, F_GETFD, 0)|O_NONBLOCK) == -1) {
		return -1;
	}
	return 0;
}
//
// 得到tcp的连接状态...
int gettcpstate(int sockfd)
{
  struct tcp_info info = {0};
  int optlen = sizeof(struct tcp_info);
  if( getsockopt(sockfd, IPPROTO_TCP, TCP_INFO, &info, (socklen_t *)&optlen) < 0 )
    return false;
  return ((info.tcpi_state == TCP_ESTABLISHED) ? true : false);
}
//
// 全新的日志处理函数...
bool do_trace(const char * inFile, int inLine, const char *msg, ...)
{
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
                "[%d-%02d-%02d %02d:%02d:%02d.%03d][%d][%s:%d] ", 1900 + tm->tm_year, 1 + tm->tm_mon, tm->tm_mday,
                tm->tm_hour, tm->tm_min, tm->tm_sec, (int)(tv.tv_usec/1000), getpid(), inFile, inLine);
  // 确认日志头长度...
  if(log_size == -1) {
    return false;
  }
  // 打开日志文件...
  int file_log_fd = open(g_absolute_path, O_RDWR|O_CREAT|O_APPEND, 0666);
  if( file_log_fd < 0 ) {
    return false;
  }
  // 对数据进行格式化...
  va_list vl_list;
  va_start(vl_list, msg);
  log_size += vsnprintf(log_data + log_size, LOG_MAX_SIZE - log_size, msg, vl_list);
  va_end(vl_list);
  // 加入结尾符号...
  log_data[log_size++] = '\n';
  // 将格式化之后的数据写入文件...
  write(file_log_fd, log_data, log_size);
  close(file_log_fd);
  // 将数据打印到控制台...
  fprintf(stderr, log_data);
  return true;
}
//
// 获取用户类型...
const char * get_client_type(int inType)
{
  switch(inType)
  {
    case kClientPHP:    return "PHP";
    case kClientGather: return "Gather";
    case kClientLive:   return "SRS";
    case kClientPlay:   return "Player";
  }
  return "unknown";
}
//
// 获取命令类型...
const char * get_command_name(int inCmd)
{
  switch(inCmd)
  {
    case kCmd_Gather_Login:           return "Login";
    case kCmd_PHP_Get_Camera_Status:  return "Get_Camera_Status";
    case kCmd_PHP_Set_Camera_Name:    return "Set_Camera_Name";
    case kCmd_PHP_Set_Course_Add:     return "Set_Course_Add";
    case kCmd_PHP_Set_Course_Mod:     return "Set_Course_Mod";
    case kCmd_PHP_Set_Course_Del:     return "Set_Course_Del";
    case kCmd_PHP_Get_Gather_Status:  return "Get_Gather_Status";
    case kCmd_PHP_Get_Course_Record:  return "Get_Course_Record";
    case kCmd_PHP_Get_All_Client:     return "Get_All_Client";
    case kCmd_PHP_Get_Live_Server:    return "Get_Live_Server";
    case kCmd_PHP_Start_Camera:       return "Start_Camera";
    case kCmd_PHP_Stop_Camera:        return "Stop_Camera";
    case kCmd_Live_Login:             return "Login";
    case kCmd_Live_Vary:              return "Vary";
    case kCmd_Live_Quit:              return "Quit";
    case kCmd_Play_Login:             return "Login";  
    case kCmd_Play_Verify:            return "Verify";  
  }
  return "unknown";
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
