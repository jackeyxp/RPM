
#pragma once

#include "global.h"
#include "thread.h"

class CApp : public CThread
{
public:
  CApp();
  virtual ~CApp();
  virtual void Entry();
public:
  bool        doInitRLimit();
  void        doWaitSocket();
  int         doCreateSocket(int nPort);
  void        doAddSupplyList(CTeacher * lpTeacher);
  void        doDelSupplyList(CTeacher * lpTeacher);
public:
  int         GetListenFD() { return m_listen_fd; }
  CRoom   *   doCreateRoom(int inRoomID, int inLiveID);
private:
  bool        doProcSocket(char * lpBuffer, int inBufSize, sockaddr_in & inAddr);
  void        doTagDelete(int nHostPort);
  void        doServerSendDetect();
  void        doCheckTimeout();
  int         doSendSupply();
private:
  int               m_listen_fd;      // UDP监听套接字...
  GM_MapRoom        m_MapRoom;        // 房间列表...
  GM_MapNetwork     m_MapNetwork;     // 网络对象列表...
  GM_ListTeacher    m_ListTeacher;    // 有补包的老师推流列表...
  pthread_mutex_t   m_mutex;          // 线程互斥对象...
};