
#pragma once

#include "global.h"

class CTCPThread;
class CUDPThread;
class CApp
{
public:
  CApp();
  ~CApp();
public:
  void        onSignalQuit();
  bool        check_pid_file();
  bool        acquire_pid_file();
  bool        destory_pid_file();
  bool        doStartThread();
  bool        doInitRLimit();
  bool        doInitWanAddr();
  void        doWaitUdpSocket();
  int         doCreateUdpSocket();
  int         doTCPRoomCommand(int nCmdID, int nRoomID);
  bool        doProcessCmdLine(int argc, char * argv[]);
  void        doDeleteForCameraLiveStop(int inRoomID);
  bool        IsUDPTeacherPusherOnLine(int inRoomID);
  bool        IsUDPStudentPusherOnLine(int inRoomID, int inDBCameraID);
  void        doUDPTeacherLookerDelete(int inRoomID, int inDBCameraID);
  void        doUDPTeacherPusherOnLine(int inRoomID, bool bIsOnLineFlag);
  void        doUDPStudentPusherOnLine(int inRoomID, int inDBCameraID, bool bIsOnLineFlag);
  void        doUDPLogoutToTCP(int nTCPSockFD, int nDBCameraID, uint8_t tmTag, uint8_t idTag);
public:
  int         GetListenFD() { return m_listen_fd; }
  int         GetCenterPort() { return DEF_CENTER_PORT; }
  const char* GetCenterAddr() { return DEF_CENTER_ADDR; }
  string   &  GetWanAddr() { return m_strWanAddr; }
  int         GetUdpPort() { return DEF_UDP_PORT; }
  int         GetTcpPort() { return DEF_TCP_PORT; }
  bool        IsDebugMode() { return m_bIsDebugMode; }
  bool        IsSignalQuit() { return m_signal_quit; }
private:
  int         read_pid_file();
  void        doStopSignal();
  void        clearAllSource();
private:
  bool              m_bIsDebugMode;     // 是否是调试模式 => 只能挂载调试模式的学生端和讲师端...
  int               m_listen_fd;        // UDP监听套接字...
  bool              m_signal_quit;      // 信号退出标志...
  string            m_strWanAddr;       // 本地外网地址...
  CTCPThread   *    m_lpTCPThread;      // TCP监听线程对象...
  CUDPThread   *    m_lpUDPThread;      // UDP数据线程对象...
};