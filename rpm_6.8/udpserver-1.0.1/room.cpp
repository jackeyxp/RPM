
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

void CRoom::doDumpRoomInfo()
{
  log_trace("\n======== RoomID: %d, LiveID: %d ========\n Student-Pusher: %d, Teacher-Looker: %d\n Teacher-Pusher: %d, Student-Looker: %d", m_nRoomID, m_nLiveID,
            ((m_lpStudentPusher != NULL) ? 1 : 0), ((m_lpTeacherLooker != NULL) ? 1 : 0), ((m_lpTeacherPusher != NULL) ? 1 : 0), m_MapStudentLooker.size());
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
  // ��������ѧ���˶�����Ч��ֱ�ӷ���...
  if( lpStudent == NULL )
    return;
  // ����ָ��Ƚϣ������ѧ�������ˣ��ÿգ�����...
  if( lpStudent == m_lpStudentPusher ) {
    m_lpStudentPusher = NULL;
    return;
  }
  // ��ѧ���ۿ����б�������...
  int nHostPort = lpStudent->GetHostPort();
  GM_MapStudent::iterator itorItem = m_MapStudentLooker.find(nHostPort);
  // �ҵ���عۿ�ѧ���˶���ֱ��ɾ������...
  if( itorItem != m_MapStudentLooker.end() ) {
    m_MapStudentLooker.erase(itorItem);
    return;
  }
  // ���ͨ���˿ڷ�ʽû���ҵ���ͨ��ָ���������...
  itorItem = m_MapStudentLooker.begin();
  while(itorItem != m_MapStudentLooker.end()) {
    // �ҵ�����ؽڵ� => ɾ���ڵ㣬����...
    if(itorItem->second == lpStudent) {
      m_MapStudentLooker.erase(itorItem);
      return;
    }
  }
  // ͨ��ָ�����Ҳû���ҵ�����ӡ������Ϣ...
  log_trace("Can't find Student-Looker, HostPort: %d", nHostPort);
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
    lpStudent->doTransferToStudentLooker(lpBuffer, inBufSize);
  }
  return true;
}