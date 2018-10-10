
#include "app.h"
#include "room.h"
#include "student.h"
#include "teacher.h"

CRoom::CRoom(int inRoomID)
  : m_lpStudentPusher(NULL)
  , m_lpTeacherPusher(NULL)
  , m_lpTeacherLooker(NULL)
  , m_nRoomID(inRoomID)
{
  
}

CRoom::~CRoom()
{
  
}

void CRoom::doDumpRoomInfo()
{
  log_trace("\n======== RoomID: %d ========\n Student-Pusher: %d, Teacher-Looker: %d\n Teacher-Pusher: %d, Student-Looker: %d", m_nRoomID, 
            ((m_lpStudentPusher != NULL) ? 1 : 0), ((m_lpTeacherLooker != NULL) ? 1 : 0), ((m_lpTeacherPusher != NULL) ? 1 : 0), m_MapStudentLooker.size());
}
//
// ���ݴ��ݹ�����ѧ�������ͣ����б�������...
void CRoom::doCreateStudent(CStudent * lpStudent)
{
  int nHostPort = lpStudent->GetHostPort();
  uint8_t idTag = lpStudent->GetIdTag();
  if( idTag == ID_TAG_PUSHER ) {
    // ����µ�ѧ������������ԭ�ж�����ͬ(���ܶ�η�����) => ���߷������TCP��ʦ�ˣ����Դ��������߳���...
    if( m_lpStudentPusher != lpStudent ) {
      m_lpStudentPusher = lpStudent;
      GetApp()->doUDPStudentPusherOnLine(m_nRoomID, lpStudent->GetDBCameraID(), true);
    }
    // ���µ�ѧ���������󱣴�����...
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
  // �����ѧ�������� => ���߷������TCP��ʦ�ˣ�����ɾ�������߳���...
  if( lpStudent == m_lpStudentPusher ) {
    // ����ǽ�ʦ�˷����ɾ�����ÿշ���...
    if( lpStudent->GetDeleteByTCP() ) {
      m_lpStudentPusher = NULL;
      return;
    }
    // �����ѧ���˷����ɾ����֪ͨ��ʦ�ˣ�����ɾ�������߳��ˣ�Ȼ���ÿգ�����...
    GetApp()->doUDPStudentPusherOnLine(m_nRoomID, lpStudent->GetDBCameraID(), false);
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
    // 2018.09.12 - by jackey => ��ɹ���������...
    // û���ҵ���ؽڵ� => ��������һ��...
    ++itorItem;
  }
  // ͨ��ָ�����Ҳû���ҵ�����ӡ������Ϣ...
  log_trace("Can't find UDP-Student in CRoom, HostPort: %d", nHostPort);
}
//
// ���ݴ��ݹ�������ʦ�����ͣ����б�������...
void CRoom::doCreateTeacher(CTeacher * lpTeacher)
{
  uint8_t idTag = lpTeacher->GetIdTag();
  if( idTag == ID_TAG_PUSHER ) {
    // ����µĽ�ʦ����������ԭ�ж�����ͬ(���ܶ�η�����) => ��������TCP����ѧ���ˣ����Դ��������߳���...
    if( m_lpTeacherPusher != lpTeacher ) {
      m_lpTeacherPusher = lpTeacher;
      GetApp()->doUDPTeacherPusherOnLine(m_nRoomID, true);
    }
    // ���µĽ�ʦ�������󱣴�����...
    m_lpTeacherPusher = lpTeacher;
  } else if( idTag == ID_TAG_LOOKER ) {
    m_lpTeacherLooker = lpTeacher;
  }
}
//
// ������ʦ��ɾ���¼�...
void CRoom::doDeleteTeacher(CTeacher * lpTeacher)
{
  // �������ʦ������ => ��������TCP����ѧ���ˣ�����ɾ�������߳���
  if( m_lpTeacherPusher == lpTeacher ) {
    GetApp()->doUDPTeacherPusherOnLine(m_nRoomID, false);
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