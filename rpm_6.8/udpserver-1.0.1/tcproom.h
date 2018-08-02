
#pragma once

#include "global.h"

class CTCPCamera;
class CTCPClient;
class CTCPRoom
{
public:
  CTCPRoom(int inRoomID);
  ~CTCPRoom();
public:
  void         doDeleteCamera(int nDBCameraID);
  CTCPCamera * doCreateCamera(int nDBCameraID, int nTCPSockFD, string & strCameraName, string & strPCName);
  CTCPClient * GetTCPTeacher() { return m_lpTCPTeacher; }
  GM_MapTCPCamera & GetMapCamera() { return m_MapTCPCamera; }
public:
  void         doCreateStudent(CTCPClient * lpStudent);
  void         doDeleteStudent(CTCPClient * lpStudent);
  void         doCreateTeacher(CTCPClient * lpTeacher);
  void         doDeleteTeacher(CTCPClient * lpTeacher);
  void         doUDPTeacherPusherOnLine(bool bIsOnLineFlag);
private:
  void         clearAllCamera();
private:
  int              m_nRoomID;          // 房间编号...
  CTCPClient   *   m_lpTCPTeacher;     // 只有一个老师端推流，发给多个学生端...
  GM_MapTCPConn    m_MapTCPStudent;    // 学生端观看者列表，都接收老师端推流...
  GM_MapTCPCamera  m_MapTCPCamera;     // 房间里面在线的可供拉取的摄像头列表...
};
