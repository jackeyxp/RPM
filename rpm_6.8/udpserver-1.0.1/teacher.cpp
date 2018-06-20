
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
  bool bResult = false;
  CApp * lpApp = GetApp();
  // ���´�����������ݣ���������·��䣬���·��������ʦ��...
  memcpy(&m_rtp_create, lpBuffer, sizeof(m_rtp_create));
  m_lpRoom = lpApp->doCreateRoom(m_rtp_create.roomID, m_rtp_create.liveID);
  m_lpRoom->doCreateTeacher(this);
  // �ظ���ʦ������ => �����Ѿ������ɹ�����Ҫ�ٷ�����������...
  if( this->GetIdTag() == ID_TAG_PUSHER ) {
    bResult = this->doCreateForPusher(lpBuffer, inBufSize);
  }
  // �ظ���ʦ�ۿ��� => ��ѧ�������˵�����ͷת������ʦ�ۿ��� => �ۿ����յ��󣬻ᷢ��׼����������...
  if( this->GetIdTag() == ID_TAG_LOOKER ) {
    bResult = this->doCreateForLooker(lpBuffer, inBufSize);
  }
  // ����ִ�н��...
  return bResult;
}
//
// �ظ���ʦ������ => �����Ѿ������ɹ�����Ҫ�ٷ�����������...
bool CTeacher::doCreateForPusher(char * lpBuffer, int inBufSize)
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
	sockaddr_in addrTeacher = {0};
	addrTeacher.sin_family = AF_INET;
	addrTeacher.sin_port = htons(nHostPort);
	addrTeacher.sin_addr.s_addr = htonl(nHostAddr);
  // �ظ���ʦ������ => �����Ѿ������ɹ�����Ҫ�ٷ�����������...
  if( sendto(listen_fd, &rtpHdr, sizeof(rtpHdr), 0, (sockaddr*)&addrTeacher, sizeof(addrTeacher)) < 0 ) {
    log_trace("sendto error(code:%d, %s)", errno, strerror(errno));
    return false;
  }
  // û�д���ֱ�ӷ���...
  return true;
}
//
// �ظ���ʦ�ۿ��� => ��ѧ�������˵�����ͷת������ʦ�ۿ��� => �ۿ����յ��󣬻ᷢ��׼����������...
bool CTeacher::doCreateForLooker(char * lpBuffer, int inBufSize)
{
  // ���û�з��䣬ֱ�ӷ���...
  if( m_lpRoom == NULL )
    return false;
  // ��ȡ�������ѧ�������߶��� => �������ߣ�ֱ�ӷ���...
  CStudent * lpStudent = m_lpRoom->GetStudentPusher();
  if( lpStudent == NULL )
    return false;
  // ��ȡѧ�������ߵ�����ͷ��Ϣ => ����ͷΪ�գ�ֱ�ӷ���...
  string & strSeqHeader = lpStudent->GetSeqHeader();
  if( strSeqHeader.size() <= 0 )
    return false;
  // ��ȡ��Ҫ����ر�����Ϣ...
  uint32_t nHostAddr = this->GetHostAddr();
  uint16_t nHostPort = this->GetHostPort();
  int listen_fd = GetApp()->GetListenFD();
  if( listen_fd <= 0 || nHostAddr <= 0 || nHostPort <= 0 )
    return false;
  // ���챾�����Լ��Ľ��յ�ַ...
	sockaddr_in addrTeacher = {0};
	addrTeacher.sin_family = AF_INET;
	addrTeacher.sin_port = htons(nHostPort);
	addrTeacher.sin_addr.s_addr = htonl(nHostAddr);
  // �ظ���ʦ�ۿ��� => ��ѧ�������˵�����ͷת������ʦ�ۿ���...
  if( sendto(listen_fd, strSeqHeader.c_str(), strSeqHeader.size(), 0, (sockaddr*)&addrTeacher, sizeof(addrTeacher)) < 0 ) {
    log_trace("sendto error(code:%d, %s)", errno, strerror(errno));
    return false;
  }
  // ��ӡ��������������ʦ�ۿ��˱���������Ϣ...
  //log_debug("[Teacher] doCreateForLooker => %lu:%lu", nHostAddr, nHostPort);
  // û�д���ֱ�ӷ���...
  return true;
}

bool CTeacher::doTagDelete(char * lpBuffer, int inBufSize)
{
  return true;
}

bool CTeacher::doTagSupply(char * lpBuffer, int inBufSize)
{
  // ֻ����ʦ�ۿ��߲Żᷢ�𲹰�����...
  if( this->GetIdTag() != ID_TAG_LOOKER )
    return false;
  // ����������ת����ѧ��������...
  return this->doTransferToStudentPusher(lpBuffer, inBufSize);
}

bool CTeacher::doTagHeader(char * lpBuffer, int inBufSize)
{
  // ֻ����ʦ�����߲Żᴦ������ͷ����...
  if( this->GetIdTag() != ID_TAG_PUSHER )
    return false;
  // ��ʦ��������Ҫ������ѧ���ˣ�ֱ�ӱ�������ͷ����ѧ���ۿ��˽���ʱ����ת������Ӧ��ѧ����...
  m_strSeqHeader.assign(lpBuffer, inBufSize);
  // �ظ���ʦ������ => ����ͷ�Ѿ��յ�����Ҫ�ٷ�����ͷ������...
  return this->doHeaderForPusher(lpBuffer, inBufSize);
}
//
// �ظ���ʦ������ => ����ͷ�Ѿ��յ�����Ҫ�ٷ�����ͷ������...
bool CTeacher::doHeaderForPusher(char * lpBuffer, int inBufSize)
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
	sockaddr_in addrTeacher = {0};
	addrTeacher.sin_family = AF_INET;
	addrTeacher.sin_port = htons(nHostPort);
	addrTeacher.sin_addr.s_addr = htonl(nHostAddr);
  // �ظ���ʦ������ => ����ͷ�Ѿ��յ�����Ҫ�ٷ�����ͷ������...
  if( sendto(listen_fd, &rtpHdr, sizeof(rtpHdr), 0, (sockaddr*)&addrTeacher, sizeof(addrTeacher)) < 0 ) {
    log_trace("sendto error(code:%d, %s)", errno, strerror(errno));
    return false;
  }
  // û�д���ֱ�ӷ���...
  return true;
}

bool CTeacher::doTagReady(char * lpBuffer, int inBufSize)
{
  bool bResult = false;
  // ��ʦ�ۿ��� => ת��ѧ�������ߣ��Ѿ��յ�����ͷ����ۿ����Ѿ�׼������...
  if( this->GetIdTag() == ID_TAG_LOOKER ) {
    // ���Լ��Ĵ�͸��ַ���µ�׼�������ṹ�嵱��...
    rtp_ready_t * lpLookReady = (rtp_ready_t*)lpBuffer;
    lpLookReady->recvPort = this->GetHostPort();
    lpLookReady->recvAddr = this->GetHostAddr();
    // ת��ѧ�������ߣ��Ѿ��յ�����ͷ����ۿ����Ѿ�׼������...
    bResult = this->doTransferToStudentPusher(lpBuffer, inBufSize);
  }
  // ע�⣺������Ҫ���⴦�� => ѧ���ۿ��߿����Ƕ������Ҫ��ʦ������ָ���ظ��ĸ�ѧ���ۿ���...
  // ע�⣺ָ����������յ��� rtp_ready_t �ṹ�嵱��...
  // ��ʦ������ => �ظ�ѧ���ۿ��ߣ��Ѿ��յ�׼�����������Ҫ�ٷ���׼������������...
  //if( this->GetIdTag() == ID_TAG_PUSHER ) {
  //  bResult = this->doTransferToStudentLooker(lpBuffer, inBufSize);
  //}
  // ����ִ�н��...
  return bResult;
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