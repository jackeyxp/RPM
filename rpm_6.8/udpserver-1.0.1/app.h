
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
  void        doWaitSocket();
  int         doCreateSocket(int nPort);
  void        doAddLoseForStudent(CStudent * lpStudent);
  void        doDelLoseForStudent(CStudent * lpStudent);
  void        doAddSupplyForTeacher(CTeacher * lpTeacher);
  void        doDelSupplyForTeacher(CTeacher * lpTeacher);
  bool        IsUDPTeacherPusherOnLine(int inRoomID);
  void        doUDPTeacherPusherOnLine(int inRoomID, bool bIsOnLineFlag);
  void        doLogoutForUDP(int nTCPSockFD, int nDBCameraID, uint8_t tmTag, uint8_t idTag);
public:
  CRoom   *   doCreateRoom(int inRoomID);
  int         GetListenFD() { return m_listen_fd; }
private:
  bool        doProcSocket(char * lpBuffer, int inBufSize, sockaddr_in & inAddr);
  void        doTagDelete(int nHostPort);
  void        doServerSendDetect();
  void        doCheckTimeout();
  int         doSendSupply();
  int         doSendLose();
private:
  int               m_listen_fd;      // UDP监听套接字...
  GM_MapRoom        m_MapRoom;        // 房间列表...
  GM_MapNetwork     m_MapNetwork;     // 网络对象列表...
  GM_ListTeacher    m_ListTeacher;    // 有补包的老师推流者列表...
  GM_ListStudent    m_ListStudent;    // 有丢包的学生观看者列表...
  pthread_mutex_t   m_mutex;          // 线程互斥对象...
  os_sem_t     *    m_sem_t;          // 辅助线程信号量...
  CTCPThread   *    m_lpTCPThread;    // TCP监听线程对象...
};