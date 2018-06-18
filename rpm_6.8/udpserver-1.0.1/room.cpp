
#include "room.h"
#include "student.h"
#include "teacher.h"

CRoom::CRoom(int inRoomID, int inLiveID)
  : m_lpStudentPusher(NULL)
  , m_lpTeacherPusher(NULL)
  , m_lpTeacherLooker(NULL)
  , m_nRoomID(inRoomID)
  , m_nLiveID(inLiveID)
{
  
}

CRoom::~CRoom()
{
  
}
//
// ���ݴ��ݹ�����ѧ�������ͣ����б�������...
void CRoom::doCreateStudent(CStudent * lpStudent)
{
  int nHostPort = lpStudent->GetHostPort();
  uint8_t idTag = lpStudent->GetIdTag();
  if( idTag == ID_TAG_PUSHER ) {
    m_lpStudentPusher = lpStudent;
  } else if( idTag == ID_TAG_LOOKER ) {
    m_MapStudentLooker[nHostPort] = lpStudent;
  }
}
//
// ����ѧ����ɾ���¼�...
void CRoom::doDeleteStudent(CStudent * lpStudent)
{
  // ����ָ��Ƚϣ������ѧ�������ˣ��ÿգ�����...
  if( lpStudent == m_lpStudentPusher ) {
    m_lpStudentPusher = NULL;
    return;
  }
  // ��ѧ���ۿ����б�������...
  GM_MapStudent::iterator itorItem = m_MapStudentLooker.begin();
  while(itorItem != m_MapStudentLooker.end()) {
    // �ҵ�����ؽڵ� => ɾ���ڵ㣬����...
    if(itorItem->second == lpStudent) {
      m_MapStudentLooker.erase(itorItem);
      return;
    }
  }
}
//
// ���ݴ��ݹ�������ʦ�����ͣ����б�������...
void CRoom::doCreateTeacher(CTeacher * lpTeacher)
{
  uint8_t idTag = lpTeacher->GetIdTag();
  if( idTag == ID_TAG_PUSHER ) {
    m_lpTeacherPusher = lpTeacher;
  } else if( idTag == ID_TAG_LOOKER ) {
    m_lpTeacherLooker = lpTeacher;
  }
}
//
// ������ʦ��ɾ���¼�...
void CRoom::doDeleteTeacher(CTeacher * lpTeacher)
{
  // �������ʦ�����ˣ��ÿգ�����...
  if( m_lpTeacherPusher == lpTeacher ) {
    m_lpTeacherPusher = NULL;
    return;
  }
  // �������ʦ�ۿ��ˣ��ÿգ�����...
  if( m_lpTeacherLooker == lpTeacher ) {
    m_lpTeacherLooker = NULL;
    return;
  }
}

bool CRoom::doTransferToStudentLooker(char * lpBuffer, int inBufSize)
{
  if( lpBuffer == NULL || inBufSize <= 0 )
    return false;
  if( m_MapStudentLooker.size() <= 0 )
    return false;
  GM_MapStudent::iterator itorItem;
  for(itorItem = m_MapStudentLooker.begin(); itorItem != m_MapStudentLooker.end(); ++itorItem) {
    CStudent * lpStudent = itorItem->second;
    if( lpStudent == NULL ) continue;
    lpStudent->doTransferByRoom(lpBuffer, inBufSize);
  }
  return true;
}