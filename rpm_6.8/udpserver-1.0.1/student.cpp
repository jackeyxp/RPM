
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

bool CStudent::doServerSendDetect()
{
  // ע�⣺ѧ���˶������з���������̽��...
  return false;  
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
  bool bResult = false;
  CApp * lpApp = GetApp();
  // ���´�����������ݣ���������·��䣬���·������ѧ����...
  memcpy(&m_rtp_create, lpBuffer, sizeof(m_rtp_create));
  m_lpRoom = lpApp->doCreateRoom(m_rtp_create.roomID, m_rtp_create.liveID);
  m_lpRoom->doCreateStudent(this);
  // �ظ�ѧ�������� => �����Ѿ������ɹ�����Ҫ�ٷ�����������...
  if( this->GetIdTag() == ID_TAG_PUSHER ) {
    bResult = this->doCreateForPusher(lpBuffer, inBufSize);
  }
  // �ظ�ѧ���ۿ��� => ����ʦ�����˵�����ͷת����ѧ���ۿ��� => �ۿ����յ��󣬻ᷢ��׼����������...
  if( this->GetIdTag() == ID_TAG_LOOKER ) {
    bResult = this->doCreateForLooker(lpBuffer, inBufSize);
  }
  // ����ִ�н��...
  return bResult;
}
//
// �ظ�ѧ�������� => �����Ѿ������ɹ�����Ҫ�ٷ�����������...
bool CStudent::doCreateForPusher(char * lpBuffer, int inBufSize)
{
  // ���췴�������ݰ�...
  rtp_hdr_t rtpHdr = {0};
  rtpHdr.tm = TM_TAG_SERVER;
  rtpHdr.id = ID_TAG_SERVER;
  rtpHdr.pt = PT_TAG_CREATE;
  // ��ȡ��Ҫ����ر�����Ϣ...
  uint32_t nHostAddr = this->GetHostAddr();
  uint16_t nHostPort = this->GetHostPort();
  int listen_fd = GetApp()->GetListenFD();
  if( listen_fd <= 0 || nHostAddr <= 0 || nHostPort <= 0 )
    return false;
  // ���챾�����Լ��Ľ��յ�ַ...
	sockaddr_in addrStudent = {0};
	addrStudent.sin_family = AF_INET;
	addrStudent.sin_port = htons(nHostPort);
	addrStudent.sin_addr.s_addr = htonl(nHostAddr);
  // �ظ�ѧ�������� => �����Ѿ������ɹ�����Ҫ�ٷ�����������...
  if( sendto(listen_fd, &rtpHdr, sizeof(rtpHdr), 0, (sockaddr*)&addrStudent, sizeof(addrStudent)) < 0 ) {
    log_trace("sendto error(code:%d, %s)", errno, strerror(errno));
    return false;
  }
  // ��ӡ������������ѧ�������˱���������Ϣ...
  //log_debug("[Student] doCreateForPusher => %u:%u", nHostAddr, nHostPort);
  // û�д���ֱ�ӷ���...
  return true;
}
//
// �ظ�ѧ���ۿ��� => ����ʦ�����˵�����ͷת����ѧ���ۿ��� => �ۿ����յ��󣬻ᷢ��׼����������...
bool CStudent::doCreateForLooker(char * lpBuffer, int inBufSize)
{
  // ���û�з��䣬ֱ�ӷ���...
  if( m_lpRoom == NULL )
    return false;
  // ��ȡ���������ʦ�����߶��� => �������ߣ�ֱ�ӷ���...
  CTeacher * lpTeacher = m_lpRoom->GetTeacherPusher();
  if( lpTeacher == NULL )
    return false;
  // ��ȡ��ʦ�����ߵ�����ͷ��Ϣ => ����ͷΪ�գ�ֱ�ӷ���...
  string & strSeqHeader = lpTeacher->GetSeqHeader();
  if( strSeqHeader.size() <= 0 )
    return false;
  // ��ȡ��Ҫ����ر�����Ϣ...
  uint32_t nHostAddr = this->GetHostAddr();
  uint16_t nHostPort = this->GetHostPort();
  int listen_fd = GetApp()->GetListenFD();
  if( listen_fd <= 0 || nHostAddr <= 0 || nHostPort <= 0 )
    return false;
  // ���챾�����Լ��Ľ��յ�ַ...
	sockaddr_in addrStudent = {0};
	addrStudent.sin_family = AF_INET;
	addrStudent.sin_port = htons(nHostPort);
	addrStudent.sin_addr.s_addr = htonl(nHostAddr);
  // �ظ���ʦ�ۿ��� => ��ѧ�������˵�����ͷת������ʦ�ۿ���...
  if( sendto(listen_fd, strSeqHeader.c_str(), strSeqHeader.size(), 0, (sockaddr*)&addrStudent, sizeof(addrStudent)) < 0 ) {
    log_trace("sendto error(code:%d, %s)", errno, strerror(errno));
    return false;
  }
  // û�д���ֱ�ӷ���...
  return true;
}

bool CStudent::doTagDelete(char * lpBuffer, int inBufSize)
{
  // ע�⣺ɾ�������Ѿ���CApp::doTagDelete()�����ش�����...
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
  // ������ͷ�����������ȴ��ۿ��˽���ʱ��ת�����ۿ���...
  m_strSeqHeader.assign(lpBuffer, inBufSize);
  // �ظ�ѧ�������� => ����ͷ�Ѿ��յ�����Ҫ�ٷ�����ͷ������...
  return this->doHeaderForPusher(lpBuffer, inBufSize);
}
//
// �ظ�ѧ�������� => ����ͷ�Ѿ��յ�����Ҫ�ٷ�����ͷ������...
bool CStudent::doHeaderForPusher(char * lpBuffer, int inBufSize)
{
  // ���췴�������ݰ�...
  rtp_hdr_t rtpHdr = {0};
  rtpHdr.tm = TM_TAG_SERVER;
  rtpHdr.id = ID_TAG_SERVER;
  rtpHdr.pt = PT_TAG_HEADER;
  // ��ȡ��Ҫ����ر�����Ϣ...
  uint32_t nHostAddr = this->GetHostAddr();
  uint16_t nHostPort = this->GetHostPort();
  int listen_fd = GetApp()->GetListenFD();
  if( listen_fd <= 0 || nHostAddr <= 0 || nHostPort <= 0 )
    return false;
  // ���챾�����Լ��Ľ��յ�ַ...
	sockaddr_in addrStudent = {0};
	addrStudent.sin_family = AF_INET;
	addrStudent.sin_port = htons(nHostPort);
	addrStudent.sin_addr.s_addr = htonl(nHostAddr);
  // �ظ�ѧ�������� => ����ͷ�Ѿ��յ�����Ҫ�ٷ�����ͷ������...
  if( sendto(listen_fd, &rtpHdr, sizeof(rtpHdr), 0, (sockaddr*)&addrStudent, sizeof(addrStudent)) < 0 ) {
    log_trace("sendto error(code:%d, %s)", errno, strerror(errno));
    return false;
  }
  // û�д���ֱ�ӷ���...
  return true;
}

bool CStudent::doTagReady(char * lpBuffer, int inBufSize)
{
  bool bResult = false;
  // ѧ�������� => �ظ���ʦ�ۿ��ߣ��Ѿ��յ�׼�����������Ҫ�ٷ���׼������������...
  if( this->GetIdTag() == ID_TAG_PUSHER ) {
    // ���Լ��Ĵ�͸��ַ���µ�׼�������ṹ�嵱��...
    rtp_ready_t * lpPushReady = (rtp_ready_t*)lpBuffer;
    lpPushReady->recvPort = this->GetHostPort();
    lpPushReady->recvAddr = this->GetHostAddr();
    // �ظ���ʦ�ۿ��ߣ��Ѿ��յ�׼�����������Ҫ�ٷ���׼������������...
    bResult = this->doTransferToTeacherLooker(lpBuffer, inBufSize);
  }
  // ѧ���ۿ��� => ת����ʦ�����ߣ��Ѿ��յ�����ͷ����ۿ����Ѿ�׼������...
  if( this->GetIdTag() == ID_TAG_LOOKER ) {
    // ���Լ��Ĵ�͸��ַ���µ�׼�������ṹ�嵱��...
    rtp_ready_t * lpLookReady = (rtp_ready_t*)lpBuffer;
    lpLookReady->recvPort = this->GetHostPort();
    lpLookReady->recvAddr = this->GetHostAddr();
    // ת����ʦ�����ߣ��Ѿ��յ�����ͷ����ۿ����Ѿ�׼������...
    bResult = this->doTransferToTeacherPusher(lpBuffer, inBufSize);
  }
  // ����ִ�н��...
  return bResult;
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