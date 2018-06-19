
#include "app.h"
#include "room.h"
#include "student.h"
#include "teacher.h"

CStudent::CStudent(uint8_t tmTag, uint8_t idTag, uint32_t inHostAddr, uint16_t inHostPort)
  : CNetwork(tmTag, idTag, inHostAddr, inHostPort)
{
  // ��ӡѧ���˱�������Ϣ...
  log_debug("[Student-Create] tmTag: %d, idTag: %d", tmTag, idTag);
}

CStudent::~CStudent()
{
  // ��ӡѧ���˱�ɾ����Ϣ...
  log_debug("[Student-Delete] tmTag: %d, idTag: %d", this->GetTmTag(), this->GetIdTag());
  // �ڷ�����ע����ѧ���˶���...
  if( m_lpRoom != NULL ) {
    m_lpRoom->doDeleteStudent(this);
  }
}

bool CStudent::doTagDetect(char * lpBuffer, int inBufSize)
{
  bool bResult = false;
  // ѧ�������� => ������̽�����ת������ʦ�ۿ���...
  // ע�⣺������̽��� => ѧ���������Լ�̽�� + ѧ��������ת����ʦ�ۿ���̽��...
  if( this->GetIdTag() == ID_TAG_PUSHER ) {
    bResult = this->doTransferToTeacherLooker(lpBuffer, inBufSize);
  }
  // ѧ���ۿ��� => ������̽�����ת������ʦ������...
  // ע�⣺������̽��� => ѧ���ۿ����Լ�̽�� + ѧ���ۿ���ת����ʦ������̽��...
  if( this->GetIdTag() == ID_TAG_LOOKER ) {
    bResult = this->doTransferToTeacherPusher(lpBuffer, inBufSize);
  }
  // ����ִ�н��...
  return bResult;
}

bool CStudent::doTagCreate(char * lpBuffer, int inBufSize)
{
  //////////////////////////////////////////////////////
  // ע�⣺ѧ�������ߺ�ѧ���ۿ��ߴ���ʽ���в�ͬ...
  //////////////////////////////////////////////////////
  CApp * lpApp = GetApp();
  memcpy(&m_rtp_create, lpBuffer, sizeof(m_rtp_create));
  m_lpRoom = lpApp->doCreateRoom(m_rtp_create.roomID, m_rtp_create.liveID);
  m_lpRoom->doCreateStudent(this);
  return true;
}

bool CStudent::doTagDelete(char * lpBuffer, int inBufSize)
{
  return true;
}

bool CStudent::doTagSupply(char * lpBuffer, int inBufSize)
{
  // ֻ��ѧ���ۿ��߲Żᷢ�𲹰�����
  if( this->GetIdTag() != ID_TAG_LOOKER )
    return false;
  // ����������ת������ʦ������...
  return this->doTransferToTeacherPusher(lpBuffer, inBufSize);
}

bool CStudent::doTagHeader(char * lpBuffer, int inBufSize)
{
  // ֻ��ѧ�������˲Żᴦ������ͷ����...
  if( this->GetIdTag() != ID_TAG_PUSHER )
    return false;
  // ת������ͷ�����ʦ�ۿ��˶���...
  return this->doTransferToTeacherLooker(lpBuffer, inBufSize);
}

bool CStudent::doTagReady(char * lpBuffer, int inBufSize)
{
  // ֻ��ѧ���ۿ��߲Żᴦ��׼����������...
  if( this->GetIdTag() != ID_TAG_LOOKER )
    return false;
  // ת��׼�����������ʦ�����߶���...
  return this->doTransferToTeacherPusher(lpBuffer, inBufSize);
}

bool CStudent::doTagAudio(char * lpBuffer, int inBufSize)
{
  // ֻ��ѧ�������˲Żᴦ����Ƶ������...
  if( this->GetIdTag() != ID_TAG_PUSHER )
    return false;
  // ת����Ƶ�������ʦ�ۿ��˶���...
  return this->doTransferToTeacherLooker(lpBuffer, inBufSize);
}

bool CStudent::doTagVideo(char * lpBuffer, int inBufSize)
{
  // ֻ��ѧ�������˲Żᴦ����Ƶ������...
  if( this->GetIdTag() != ID_TAG_PUSHER )
    return false;
  // ת����Ƶ�������ʦ�ۿ��˶���...
  return this->doTransferToTeacherLooker(lpBuffer, inBufSize);
}

bool CStudent::doTransferToTeacherLooker(char * lpBuffer, int inBufSize)
{
  // ���û�з��䣬ֱ�ӷ���...
  if( m_lpRoom == NULL )
    return false;
  // ��ȡ���������ʦ�ۿ��߶��� => �޹ۿ��ߣ�ֱ�ӷ���...
  CTeacher * lpTeacher = m_lpRoom->GetTeacherLooker();
  if( lpTeacher == NULL )
    return false;
  // ��ȡ��Ҫ����ر�����Ϣ...
  uint32_t nHostAddr = lpTeacher->GetHostAddr();
  uint16_t nHostPort = lpTeacher->GetHostPort();
  int listen_fd = GetApp()->GetListenFD();
  if( listen_fd <= 0 || nHostAddr <= 0 || nHostPort <= 0 )
    return false;
  // ����ۿ��ߵĽ��յ�ַ...
	sockaddr_in addrTeacher = {0};
	addrTeacher.sin_family = AF_INET;
	addrTeacher.sin_port = htons(nHostPort);
	addrTeacher.sin_addr.s_addr = htonl(nHostAddr);
  // ������ͷͨ���������ת������ʦ�ۿ���...
  if( sendto(listen_fd, lpBuffer, inBufSize, 0, (sockaddr*)&addrTeacher, sizeof(addrTeacher)) < 0 ) {
    log_trace("sendto error(code:%d, %s)", errno, strerror(errno));
    return false;
  }
  // û�д���ֱ�ӷ���...
  return true;
}

bool CStudent::doTransferToTeacherPusher(char * lpBuffer, int inBufSize)
{
  // ���û�з��䣬ֱ�ӷ���...
  if( m_lpRoom == NULL )
    return false;
  // ��ȡ���������ʦ�����߶��� => �������ߣ�ֱ�ӷ���...
  CTeacher * lpTeacher = m_lpRoom->GetTeacherPusher();
  if( lpTeacher == NULL )
    return false;
  // ��ȡ��Ҫ����ر�����Ϣ...
  uint32_t nHostAddr = lpTeacher->GetHostAddr();
  uint16_t nHostPort = lpTeacher->GetHostPort();
  int listen_fd = GetApp()->GetListenFD();
  if( listen_fd <= 0 || nHostAddr <= 0 || nHostPort <= 0 )
    return false;
  // ����ۿ��ߵĽ��յ�ַ...
	sockaddr_in addrTeacher = {0};
	addrTeacher.sin_family = AF_INET;
	addrTeacher.sin_port = htons(nHostPort);
	addrTeacher.sin_addr.s_addr = htonl(nHostAddr);
  // ������ͷͨ���������ת������ʦ�ۿ���...
  if( sendto(listen_fd, lpBuffer, inBufSize, 0, (sockaddr*)&addrTeacher, sizeof(addrTeacher)) < 0 ) {
    log_trace("sendto error(code:%d, %s)", errno, strerror(errno));
    return false;
  }
  // û�д���ֱ�ӷ���...
  return true;
}

bool CStudent::doTransferByRoom(char * lpBuffer, int inBufSize)
{
  if( lpBuffer == NULL || inBufSize <= 0 )
    return false;
   // ��ȡ��Ҫ����ر�����Ϣ...
  uint32_t nHostAddr = this->GetHostAddr();
  uint16_t nHostPort = this->GetHostPort();
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