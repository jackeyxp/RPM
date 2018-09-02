
#pragma once

#include "global.h"

class CTCPClient;
class CTCPThread;
class CUdpServer;
class CRoom;
class CApp
{
public:
  CApp();
  ~CApp();
public:
  bool         doStartThread();
  bool         doInitRLimit();
  void         doWaitForExit();
public:
  CUdpServer * doFindMinUdpServer();
  CUdpServer * doFindUdpServer(int inSocketFD);
  CUdpServer * doCreateUdpServer(int inSocketFD);
  void         doDeleteUdpServer(int inSocketFD);
  CTCPRoom  *  doFindTCPRoom(int nRoomID);
  CTCPRoom  *  doCreateRoom(int nRoomID, CUdpServer * lpUdpServer);
  void         doDeleteRoom(CUdpServer * lpUdpServer);
  void         doDeleteRoom(int nRoomID);
private:
  os_sem_t     *    m_sem_t;          // 辅助线程信号量...
  CTCPThread   *    m_lpTCPThread;    // TCP监听线程对象...
  GM_MapServer      m_MapServer;      // UDP服务器集合列表...
  GM_MapRoom        m_MapRoom;        // 房间对象集合列表...
};

class CUdpServer
{
public:
  CUdpServer();
  ~CUdpServer();
public:
  int          GetRoomCount() { return m_MapRoom.size(); }
public:
  void         doMountRoom(CTCPRoom * lpRoom);
  void         doUnMountRoom(int nRoomID);
  void         doAddTeacher(int nRoomID);
  void         doDelTeacher(int nRoomID);
  void         doAddStudent(int nRoomID);
  void         doDelStudent(int nRoomID);
private:
  GM_MapRoom   m_MapRoom;         // 已挂载房间对象集合列表...
  string       m_strRemoteAddr;   // 服务器远程地址
  string       m_strUdpAddr;      // 服务器UDP地址
  int          m_nRemotePort;     // 服务器远程端口
  int          m_nUdpPort;        // 服务器UDP端口
  
  friend class CTCPClient;
};

class CTCPRoom
{
public:
  CTCPRoom(int nRoomID, CUdpServer * lpUdpServer);
  ~CTCPRoom();
public:
  CUdpServer * GetUdpServer() { return m_lpUdpServer; }
  int          GetTeacherCount() { return m_nTeacherCount; }
  int          GetStudentCount() { return m_nStudentCount; }
  int          GetRoomID() { return m_nRoomID; }
private:
  int          m_nRoomID;
  int          m_nTeacherCount;
  int          m_nStudentCount;
  CUdpServer * m_lpUdpServer;

  friend class CUdpServer;
};
