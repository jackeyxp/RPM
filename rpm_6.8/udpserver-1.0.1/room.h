
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
  bool         doStudentPusherToStudentLooker(char * lpBuffer, int inBufSize);
  bool         doTeacherPusherToStudentLooker(char * lpBuffer, int inBufSize);
private:
  uint16_t     AddExAudioChangeNum() { return ++m_wExAudioChangeNum; }
  uint16_t     GetExAudioChangeNum() { return m_wExAudioChangeNum; }
private:
  int             m_nRoomID;            // �����ʶ����...
  uint16_t        m_wExAudioChangeNum;  // ��չ��Ƶ�仯���� => �Զ�����ػ�...
  CStudent   *    m_lpStudentPusher;    // ֻ��һ��ѧ��������������һ����ʦ��...
  CTeacher   *    m_lpTeacherLooker;    // ����ѧ��������...
  CTeacher   *    m_lpTeacherPusher;    // ֻ��һ����ʦ���������������ѧ����...
  GM_MapStudent   m_MapStudentLooker;   // ѧ���˹ۿ����б���������ʦ������...
};