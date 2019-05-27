
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
  void         ResetRoomFlow() { m_nUpFlowByte = 0; m_nDownFlowByte = 0; }
  int          GetUpFlowMB() { return m_nUpFlowByte/1000/1000; }
  int          GetDownFlowMB() { return m_nDownFlowByte/1000/1000; }
  CTeacher  *  GetTeacherLooker() { return m_lpTeacherLooker; }
  CTeacher  *  GetTeacherPusher() { return m_lpTeacherPusher; }
  CStudent  *  GetStudentPusher() { return m_lpStudentPusher; }
public:
  void         doAddDownFlowByte(int nDownSize) { m_nDownFlowByte += nDownSize; }
  void         doAddUpFlowByte(int nUpSize) { m_nUpFlowByte += nUpSize; }
  void         doDumpRoomInfo();
  void         doCreateStudent(CStudent * lpStudent);
  void         doDeleteStudent(CStudent * lpStudent);
  void         doCreateTeacher(CTeacher * lpTeacher);
  void         doDeleteTeacher(CTeacher * lpTeacher);
  bool         doStudentPusherToStudentLooker(char * lpBuffer, int inBufSize);
  bool         doTeacherPusherToStudentLooker(char * lpBuffer, int inBufSize);
private:
  uint16_t     AddExAudioChangeNum() { return ++m_wExAudioChangeNum; }
  uint16_t     GetExAudioChangeNum() { return m_wExAudioChangeNum; }
private:
  int             m_nRoomID;            // �����ʶ����...
  uint64_t        m_nUpFlowByte;        // ������������...
  uint64_t        m_nDownFlowByte;      // ������������...
  uint16_t        m_wExAudioChangeNum;  // ��չ��Ƶ�仯���� => �Զ�����ػ�...
  CStudent   *    m_lpStudentPusher;    // ֻ��һ��ѧ��������������һ����ʦ��...
  CTeacher   *    m_lpTeacherLooker;    // ����ѧ��������...
  CTeacher   *    m_lpTeacherPusher;    // ֻ��һ����ʦ���������������ѧ����...
  GM_MapStudent   m_MapStudentLooker;   // ѧ���˹ۿ����б���������ʦ������...
};