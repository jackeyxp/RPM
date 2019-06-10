
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
  int             m_nRoomID;            // �����ʶ����...
  int             m_nFocusPusherID;     // ����ѧ������������...
  uint64_t        m_nUpFlowByte;        // ������������...
  uint64_t        m_nDownFlowByte;      // ������������...
  uint16_t        m_wExAudioChangeNum;  // ��չ��Ƶ�仯���� => �Զ�����ػ�...
  CTeacher   *    m_lpTeacherPusher;    // ֻ��һ����ʦ���������������ѧ����...
  GM_MapStudent   m_MapStudentPusher;   // ѧ���������б� => DBCameraID => CStudent*
  GM_MapTeacher   m_MapTeacherLooker;   // ��ʦ�ۿ����б� => UDPPortID  => CTeacher*
};