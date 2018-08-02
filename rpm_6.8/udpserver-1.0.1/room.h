
#pragma once

#include "global.h"

class CTeacher;
class CStudent;
class CRoom
{
public:
  CRoom(int inRoomID);
  ~CRoom();
public:
  CTeacher  *  GetTeacherLooker() { return m_lpTeacherLooker; }
  CTeacher  *  GetTeacherPusher() { return m_lpTeacherPusher; }
  CStudent  *  GetStudentPusher() { return m_lpStudentPusher; }
public:
  void         doDumpRoomInfo();
  void         doCreateStudent(CStudent * lpStudent);
  void         doDeleteStudent(CStudent * lpStudent);
  void         doCreateTeacher(CTeacher * lpTeacher);
  void         doDeleteTeacher(CTeacher * lpTeacher);
  bool         doTransferToStudentLooker(char * lpBuffer, int inBufSize);
private:
  int             m_nRoomID;            // 房间标识号码...
  CStudent   *    m_lpStudentPusher;    // 只有一个学生端推流，发给一个老师端...
  CTeacher   *    m_lpTeacherLooker;    // 接收学生端推流...
  CTeacher   *    m_lpTeacherPusher;    // 只有一个老师端推流，发给多个学生端...
  GM_MapStudent   m_MapStudentLooker;   // 学生端观看者列表，都接收老师端推流...
};