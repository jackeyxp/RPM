
#pragma once

#include "global.h"
#include "thread.h"

class CTCPCenter;
class CTCPRoom;
class CTCPThread : public CThread
{
public:
  CTCPThread();
  virtual ~CTCPThread();
  virtual void Entry();
public:
  int         GetEpollFD() { return m_epoll_fd; }
  GM_MapTCPConn & GetMapConnect() { return m_MapConnect; }
public:
  bool        InitThread();                     // 初始化并启动线程...
  CTCPRoom *  doCreateRoom(int inRoomID);       // 创建或更新房间对象...
  int         doRoomCommand(int nCmdID, int nRoomID);
  void        doIncreaseClient(int inSinPort, string & strSinAddr);
  void        doDecreaseClient(int inSinPort, string & strSinAddr);
  void        doUDPTeacherLookerDelete(int inRoomID, int inDBCameraID);
  void        doUDPTeacherPusherOnLine(int inRoomID, bool bIsOnLineFlag);
  void        doUDPStudentPusherOnLine(int inRoomID, int inDBCameraID, bool bIsOnLineFlag);
  void        doUDPLogoutToTCP(int nTCPSockFD, int nDBCameraID, uint8_t tmTag, uint8_t idTag);
private:
  int     doCreateListenSocket(int nHostPort);
  void    doTCPCenterEvent(int nEvent);
  int     SetNonBlocking(int sockfd);
  int     doHandleWrite(int connfd);
  int     doHandleRead(int connfd);
  int     doHandleCenterWrite();
  int     doHandleCenterRead();
  void    doHandleTimeout();
  void    doTCPListenEvent();
  void    clearAllClient();
  void    clearAllRoom();
private:
  int                 m_epoll_fd;                // epoll句柄编号...
  int                 m_listen_fd;               // TCP监听套接字...
  CTCPCenter    *     m_lpTCPCenter;             // UDP中心连接...
  GM_MapTCPRoom       m_MapTCPRoom;              // 房间列表...
  GM_MapTCPConn	      m_MapConnect;              // the global map object...
  pthread_mutex_t     m_mutex;                   // 线程互斥对象...
  int                 m_max_event;               // 当前有效的套接字个数包括监听套接字和中心连接套接字...
  int                 m_accept_count;            // 当前从网络接收到的连接个数，有效连接个数...
  struct epoll_event  m_events[MAX_EPOLL_SIZE];  // epoll事件队列...
};
