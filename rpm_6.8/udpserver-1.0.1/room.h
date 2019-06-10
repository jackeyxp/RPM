
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
  void         ResetRoomExFlow() { m_nUpFlowByte = 0; m_nDownFlowByte = 0; m_nFocusPusherID = 0; m_wExAudioChangeNum = 0; }
  uint16_t     GetExAudioChangeNum() { return m_wExAudioChangeNum; }
  int          GetFocusPusherID() { return m_nFocusPusherID; }
  int          GetUpFlowMB() { return m_nUpFlowByte/1000/1000; }
  int          GetDownFlowMB() { return m_nDownFlowByte/1000/1000; }
  CTeacher  *  GetTeacherPusher() { return m_lpTeacherPusher; }
  CStudent  *  GetStudentPusher(int nDBCameraID);
  GM_MapTeacher & GetMapTeacherLooker() { return m_MapTeacherLooker; }
public:
  void         doAddDownFlowByte(int nDownSize) { m_nDownFlowByte += nDownSize; }
  void         doAddUpFlowByte(int nUpSize) { m_nUpFlowByte += nUpSize; }
  void         doCameraFocusPusherID(int inDBCameraID);
  void         doDumpRoomInfo();
  void         doCreateStudent(CStudent * lpStudent);
  void         doDeleteStudent(CStudent * lpStudent);
  void         doCreateTeacher(CTeacher * lpTeacher);
  void         doDeleteTeacher(CTeacher * lpTeacher);
private:
  uint16_t     AddExAudioChangeNum() { return ++m_wExAudioChangeNum; }
private:
  int             m_nRoomID;            // 房间标识号码...
  int             m_nFocusPusherID;     // 房间学生推流焦点编号...
  uint64_t        m_nUpFlowByte;        // 房间上行流量...
  uint64_t        m_nDownFlowByte;      // 房间下行流量...
  uint16_t        m_wExAudioChangeNum;  // 扩展音频变化次数 => 自动溢出回还...
  CTeacher   *    m_lpTeacherPusher;    // 只有一个老师端推流，发给多个学生端...
  GM_MapStudent   m_MapStudentPusher;   // 学生推流者列表 => DBCameraID => CStudent*
  GM_MapTeacher   m_MapTeacherLooker;   // 讲师观看者列表 => UDPPortID  => CTeacher*
};