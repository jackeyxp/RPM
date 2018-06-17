
#include "app.h"
#include "room.h"
#include "student.h"
#include "teacher.h"

CTeacher::CTeacher(uint8_t tmTag, uint8_t idTag, uint32_t inHostAddr, uint16_t inHostPort)
  : CNetwork(tmTag, idTag, inHostAddr, inHostPort)
{
  m_strSeqHeader.clear();
  // ��ӡ��ʦ�˱�������Ϣ...
  log_debug("[Teacher-Create] tmTag: %d, idTag: %d", tmTag, idTag);
}

CTeacher::~CTeacher()
{
  // ��ӡ��ʦ�˱�ɾ����Ϣ...
  log_debug("[Teacher-Delete] tmTag: %d, idTag: %d", this->GetTmTag(), this->GetIdTag());
  // �ڷ�����ע������ʦ�˶���...
  if( m_lpRoom != NULL ) {
    m_lpRoom->doDeleteTeacher(this);
  }  
}

bool CTeacher::doTagDetect(char * lpBuffer, int inBufSize)
{
  bool bResult = false;
  // ��ʦ�ۿ��� => ������̽�����ת����ѧ��������...
  // ע�⣺������̽��� => ��ʦ�ۿ����Լ�̽�� + ��ʦ�ۿ���ת��ѧ��������̽��...
  if( this->GetIdTag() == ID_TAG_LOOKER ) {
    bResult = this->doTransferToStudentPusher(lpBuffer, inBufSize);
  }
  // ��ʦ������ => ������̽�����ת����ѧ���ۿ���...
  // ע�⣺������̽��� => ��ʦ�������Լ�̽�� + ��ʦ������ת��ѧ���ۿ���̽��...
  if( this->GetIdTag() == ID_TAG_PUSHER ) {
    bResult = this->doTransferToStudentLooker(lpBuffer, inBufSize);
  }
  // ����ִ�н��...
  return bResult;
}

bool CTeacher::doTagCreate(char * lpBuffer, int inBufSize)
{
  //////////////////////////////////////////////////////
  // ע�⣺��ʦ�����ߺ���ʦ�ۿ��ߴ���ʽ���в�ͬ...
  //////////////////////////////////////////////////////
  CApp * lpApp = GetApp();
  memcpy(&m_rtp_create, lpBuffer, sizeof(m_rtp_create));
  m_lpRoom = lpApp->doCreateRoom(m_rtp_create.roomID, m_rtp_create.liveID);
  m_lpRoom->doCreateTeacher(this);
  return true;
}

bool CTeacher::doTagDelete(char * lpBuffer, int inBufSize)
{
  return true;
}

bool CTeacher::doTagSupply(char * lpBuffer, int inBufSize)
{
  return true;
}

bool CTeacher::doTagHeader(char * lpBuffer, int inBufSize)
{
  // ֻ����ʦ�����߲Żᴦ������ͷ����...
  if( this->GetIdTag() != ID_TAG_PUSHER )
    return false;
  // ��ʦ��������Ҫ������ѧ���ˣ�ֱ�ӱ�������ͷ����ѧ���ۿ��˽���ʱ����ת������Ӧ��ѧ����...
  m_strSeqHeader.assign(lpBuffer, inBufSize);
  return true;
}

bool CTeacher::doTagReady(char * lpBuffer, int inBufSize)
{
  // ֻ����ʦ�ۿ��˲Żᴦ��׼����������...
  if( this->GetIdTag() != ID_TAG_LOOKER )
    return false;
  // ����͸��ַ���µ�׼�������ṹ�嵱��...
  rtp_ready_t * lpReady = (rtp_ready_t*)lpBuffer;
  lpReady->recvPort = this->GetHostPort();
  lpReady->recvAddr = this->GetHostAddr();
  // ��׼����������ת����ѧ��������...
  return this->doTransferToStudentPusher(lpBuffer, inBufSize);
}

bool CTeacher::doTagAudio(char * lpBuffer, int inBufSize)
{
  // ֻ����ʦ�����˲Żᴦ����Ƶ������...
  if( this->GetIdTag() != ID_TAG_PUSHER )
    return false;
  // ת����Ƶ��������е�ѧ���ۿ���...
  return this->doTransferToStudentLooker(lpBuffer, inBufSize);
}

bool CTeacher::doTagVideo(char * lpBuffer, int inBufSize)
{
  // ֻ����ʦ�����˲Żᴦ����Ƶ������...
  if( this->GetIdTag() != ID_TAG_PUSHER )
    return false;
  // ת����Ƶ��������е�ѧ���ۿ���...
  return this->doTransferToStudentLooker(lpBuffer, inBufSize);
}

bool CTeacher::doTransferToStudentPusher(char * lpBuffer, int inBufSize)
{
  // ���û�з��䣬ֱ�ӷ���...
  if( m_lpRoom == NULL )
    return false;
  // ��ȡ�������ѧ�������߶��� => �������ߣ�ֱ�ӷ���...
  CStudent * lpStudent = m_lpRoom->GetStudentPusher();
  if( lpStudent == NULL )
    return false;
  // ��ȡ��Ҫ����ر�����Ϣ...
  uint32_t nHostAddr = lpStudent->GetHostAddr();
  uint16_t nHostPort = lpStudent->GetHostPort();
  int listen_fd = GetApp()->GetListenFD();
  if( listen_fd <= 0 || nHostAddr <= 0 || nHostPort <= 0 )
    return false;
  // ����ۿ��ߵĽ��յ�ַ...
	sockaddr_in addrStudent = {0};
	addrStudent.sin_family = AF_INET;
	addrStudent.sin_port = htons(nHostPort);
	addrStudent.sin_addr.s_addr = htonl(nHostAddr);
  // ������ͷͨ���������ת������ʦ�ۿ���...
  if( sendto(listen_fd, lpBuffer, inBufSize, 0, (sockaddr*)&addrStudent, sizeof(addrStudent)) < 0 ) {
    log_trace("sendto error(code:%d, %s)", errno, strerror(errno));
    return false;
  }
  // û�д���ֱ�ӷ���...
  return true;
}

bool CTeacher::doTransferToStudentLooker(char * lpBuffer, int inBufSize)
{
  // ���û�з��䣬ֱ�ӷ���...
  if( m_lpRoom == NULL )
    return false;
  // ת���������ݰ������з�����Ĺۿ���...
  return m_lpRoom->doTransferToStudentLooker(lpBuffer, inBufSize);
}