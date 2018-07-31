
#pragma once

#include "global.h"

class CTCPClient;
class CTCPRoom
{
public:
  CTCPRoom(int inRoomID);
  ~CTCPRoom();
public:
  CTCPClient * GetTCPTeacher() { return m_lpTCPTeacher; }
public:
  void         doCreateStudent(CTCPClient * lpStudent);
  void         doDeleteStudent(CTCPClient * lpStudent);
  void         doCreateTeacher(CTCPClient * lpTeacher);
  void         doDeleteTeacher(CTCPClient * lpTeacher);
  void         doUDPTeacherPusherOnLine(bool bIsOnLineFlag);
private:
  int             m_nRoomID;          // 房间编号...
  CTCPClient   *  m_lpTCPTeacher;     // 只有一个老师端推流，发给多个学生端...
  GM_MapTCPConn   m_MapTCPStudent;    // 学生端观看者列表，都接收老师端推流...
};
