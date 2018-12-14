
#pragma once

#include "global.h"
#include "thread.h"

class CTCPThread;
class CApp : public CThread
{
public:
  CApp();
  virtual ~CApp();
  virtual void Entry();
public:
  bool        doStartThread();
  bool        doInitRLimit();
  bool        doInitWanAddr();
  void        doWaitSocket();
  int         doCreateUdpSocket();
  void        doProcessCmdLine(int argc, char * argv[]);
  void        doAddLoseForStudent(CStudent * lpStudent);
  void        doDelLoseForStudent(CStudent * lpStudent);
  void        doAddSupplyForTeacher(CTeacher * lpTeacher);
  void        doDelSupplyForTeacher(CTeacher * lpTeacher);
  void        doDeleteForCameraLiveStop(int inRoomID);
  bool        IsUDPTeacherPusherOnLine(int inRoomID);
  bool        IsUDPStudentPusherOnLine(int inRoomID, int inDBCameraID);
  void        doUDPTeacherPusherOnLine(int inRoomID, bool bIsOnLineFlag);
  void        doUDPStudentPusherOnLine(int inRoomID, int inDBCameraID, bool bIsOnLineFlag);
  void        doLogoutForUDP(int nTCPSockFD, int nDBCameraID, uint8_t tmTag, uint8_t idTag);
public:
  CRoom   *   doCreateRoom(int inRoomID);
  CTCPThread* GetTCPThread() { return m_lpTCPThread; }
  int         GetListenFD() { return m_listen_fd; }
  int         GetCenterPort() { return DEF_CENTER_PORT; }
  const char* GetCenterAddr() { return DEF_CENTER_ADDR; }
  string   &  GetWanAddr() { return m_strWanAddr; }
  int         GetUdpPort() { return DEF_UDP_PORT; }
  int         GetTcpPort() { return DEF_TCP_PORT; }
  bool        IsDebugMode() { return m_bIsDebugMode; }
private:
  bool        doProcSocket(char * lpBuffer, int inBufSize, sockaddr_in & inAddr);
  void        doTagDelete(int nHostPort);
  void        doServerSendDetect();
  void        doCheckTimeout();
  int         doSendSupply();
  int         doSendLose();
private:
  bool              m_bIsDebugMode;   // 是否是调试模式 => 只能挂载调试模式的学生端和讲师端...
  int               m_listen_fd;      // UDP监听套接字...
  string            m_strWanAddr;     // 本地外网地址...
  GM_MapRoom        m_MapRoom;        // 房间列表...
  GM_MapNetwork     m_MapNetwork;     // 网络对象列表...
  GM_ListTeacher    m_ListTeacher;    // 有补包的老师推流者列表...
  GM_ListStudent    m_ListStudent;    // 有丢包的学生观看者列表...
  pthread_mutex_t   m_mutex;          // 线程互斥对象...
  os_sem_t     *    m_sem_t;          // 辅助线程信号量...
  CTCPThread   *    m_lpTCPThread;    // TCP监听线程对象...
};