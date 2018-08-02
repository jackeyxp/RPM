
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
  // ����ǹۿ��ˣ����Լ��Ӷ������е���ɾ����...
  if( this->GetIdTag() == ID_TAG_LOOKER ) {
    GetApp()->doDelLoseForStudent(this);
  }
  // ��ӡѧ�������ڵķ�����Ϣ...
  if( m_lpRoom != NULL ) {
    m_lpRoom->doDumpRoomInfo();
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
  // ѧ���ۿ��� => ֻ��һ��̽��� => ѧ���ۿ����Լ���̽���...
  // ѧ���ۿ��� => ��̽���ԭ�����ظ��Լ�����������������ʱ...
  if( this->GetIdTag() == ID_TAG_LOOKER ) {
    bResult = this->doDetectForLooker(lpBuffer, inBufSize);
  }
  // ����ִ�н��...
  return bResult;
}

bool CStudent::doDetectForLooker(char * lpBuffer, int inBufSize)
{
  // ���û�з��䣬ֱ�ӷ���...
  if( m_lpRoom == NULL )
    return false;
  // ��ȡ���������ʦ�����߶��� => �������ߣ�ֱ�ӷ���...
  CTeacher * lpTeacher = m_lpRoom->GetTeacherPusher();
  if( lpTeacher == NULL )
    return false;
  // ע�⣺����Ƶ��С��Ű����ù��Ƿ�����Ч����ֻ�򵥻�ȡ��С����...
  // ����ʦ�����ߵ�ǰ��С������Ƶ���ݰ��Ÿ��µ�̽�������...
  rtp_detect_t * lpDetect = (rtp_detect_t*)lpBuffer;
  lpDetect->maxAConSeq = lpTeacher->doCalcMinSeq(true);
  lpDetect->maxVConSeq = lpTeacher->doCalcMinSeq(false);
  return this->doTransferToStudentLooker(lpBuffer, inBufSize);
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
  m_lpRoom = lpApp->doCreateRoom(m_rtp_create.roomID);
  m_lpRoom->doCreateStudent(this);
  // �ظ�ѧ�������� => �����Ѿ������ɹ�����Ҫ�ٷ�����������...
  if( this->GetIdTag() == ID_TAG_PUSHER ) {
    bResult = this->doCreateForPusher(lpBuffer, inBufSize);
  }
  // �ظ�ѧ���ۿ��� => ����ʦ�����˵�����ͷת����ѧ���ۿ���...
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
  // ���ͳɹ�...
  return true;
}
//
// �ظ�ѧ���ۿ��� => ����ʦ�����˵�����ͷת����ѧ���ۿ���...
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
  // �ظ�ѧ���ۿ��� => ����ʦ�����˵�����ͷת����ѧ���ۿ���...
  if( sendto(listen_fd, strSeqHeader.c_str(), strSeqHeader.size(), 0, (sockaddr*)&addrStudent, sizeof(addrStudent)) < 0 ) {
    log_trace("sendto error(code:%d, %s)", errno, strerror(errno));
    return false;
  }
  // ���ͳɹ�...
  return true;
}

bool CStudent::doTagDelete(char * lpBuffer, int inBufSize)
{
  // ע�⣺ɾ�������Ѿ���CApp::doTagDelete()�����ش�����...
  return true;
}

bool CStudent::doTagSupply(char * lpBuffer, int inBufSize)
{
  // �ж��������ݵ������Ƿ���Ч�������Ч��ֱ�ӷ���...
  if( lpBuffer == NULL || inBufSize <= 0 || inBufSize < sizeof(rtp_supply_t) )
    return false;
  // ֻ��ѧ���ۿ��߲Żᷢ�𲹰�����
  if( this->GetIdTag() != ID_TAG_LOOKER )
    return false;
  // ͨ����һ���ֽڵĵ�2λ���ж��ն�����...
  uint8_t tmTag = lpBuffer[0] & 0x03;
  // ��ȡ��һ���ֽڵ���2λ���õ��ն����...
  uint8_t idTag = (lpBuffer[0] >> 2) & 0x03;
  // ��ȡ��һ���ֽڵĸ�4λ���õ����ݰ�����...
  uint8_t ptTag = (lpBuffer[0] >> 4) & 0x0F;
  // ע�⣺ֻ���� ѧ���ۿ��� �����Ĳ�������...
  if( tmTag != TM_TAG_STUDENT || idTag != ID_TAG_LOOKER )
    return false;
  // ע�⣺ѧ���ۿ��˵Ĳ����ӷ������ϵ���ʦ�����ߵĻ����ȡ...
  rtp_supply_t rtpSupply = {0};
  int nHeadSize = sizeof(rtp_supply_t);
  memcpy(&rtpSupply, lpBuffer, nHeadSize);
  if( (rtpSupply.suSize <= 0) || ((nHeadSize + rtpSupply.suSize) != inBufSize) )
    return false;
  // �������ݰ����ͣ��ҵ���������...
  bool bIsAudio = (rtpSupply.suType == PT_TAG_AUDIO) ? true : false;
  GM_MapLose & theMapLose = bIsAudio ? m_AudioMapLose : m_VideoMapLose;
  // ��ȡ��Ҫ���������кţ����뵽�������е���...
  char * lpDataPtr = lpBuffer + nHeadSize;
  int    nDataSize = rtpSupply.suSize;
  while( nDataSize > 0 ) {
    uint32_t   nLoseSeq = 0;
    rtp_lose_t rtpLose = {0};
    // ��ȡ�������к�...
    memcpy(&nLoseSeq, lpDataPtr, sizeof(int));
    // �ƶ�������ָ��λ��...
    lpDataPtr += sizeof(int);
    nDataSize -= sizeof(int);
    // �鿴����������Ƿ��Ƿ�������ҲҪ���İ�...
    // �������յ���������Զ�ת��������Ͳ��ò���...
    if( this->doIsServerLose(bIsAudio, nLoseSeq) )
      continue;
    // �ǹۿ��˶�ʧ���°�����Ҫ���в������д���...
    // ������к��Ѿ����ڣ����Ӳ��������������ڣ������¼�¼...
    if( theMapLose.find(nLoseSeq) != theMapLose.end() ) {
      rtp_lose_t & theFind = theMapLose[nLoseSeq];
      theFind.lose_type = rtpSupply.suType;
      theFind.lose_seq = nLoseSeq;
      ++theFind.resend_count;
    } else {
      rtpLose.lose_seq = nLoseSeq;
      rtpLose.lose_type = rtpSupply.suType;
      rtpLose.resend_time = (uint32_t)(os_gettime_ns() / 1000000);
      theMapLose[rtpLose.lose_seq] = rtpLose;
    }
  }
  // �����������Ϊ�� => ���Ƿ������˱������Ҫ���İ�...
  if( theMapLose.size() <= 0 )
    return true;
  // ���Լ����뵽���������б���...
  GetApp()->doAddLoseForStudent(this);
  // ��ӡ���յ���������...
  log_trace("[Student-Looker] Supply Recv => Count: %d, Type: %d", rtpSupply.suSize / sizeof(int), rtpSupply.suType);
  return true;
}

bool CStudent::doIsServerLose(bool bIsAudio, uint32_t inLoseSeq)
{
  // ���û�з��䣬ֱ�ӷ���...
  if( m_lpRoom == NULL )
    return false;
  // ��ȡ���������ʦ�����߶��� => �������ߣ�ֱ�ӷ���...
  CTeacher * lpTeacher = m_lpRoom->GetTeacherPusher();
  if( lpTeacher == NULL )
    return false;
  // ����ʦ�������в鿴�Ƿ��ڶ�Ӧ�Ĳ������е���...
  return lpTeacher->doIsServerLose(bIsAudio, inLoseSeq);
}

bool CStudent::doServerSendLose()
{
  // ���͹ۿ�����Ҫ����Ƶ��ʧ���ݰ�...
  this->doSendLosePacket(true);
  // ���͹ۿ�����Ҫ����Ƶ��ʧ���ݰ�...
  this->doSendLosePacket(false);
  // �����Ƶ����Ƶ��û�ж������ݣ�����false...
  if( m_AudioMapLose.size() <= 0 && m_VideoMapLose.size() <= 0 ) {
    return false;
  }
  // ����ƵֻҪ��һ�����в�����ţ�����true...
  return true;
}

void CStudent::doSendLosePacket(bool bIsAudio)
{
  // �������ݰ����ͣ��ҵ��������ϡ����ζ���...
  GM_MapLose & theMapLose = bIsAudio ? m_AudioMapLose : m_VideoMapLose;
  // �������϶���Ϊ�գ�ֱ�ӷ���...
  if( theMapLose.size() <= 0 )
    return;
  // �ó�һ��������¼�������Ƿ��ͳɹ�����Ҫɾ�����������¼...
  // ����ۿ��ˣ�û���յ�������ݰ������ٴη��𲹰�����...
  GM_MapLose::iterator itorItem = theMapLose.begin();
  rtp_lose_t rtpLose = itorItem->second;
  theMapLose.erase(itorItem);
  // ���û�з��䣬ֱ�ӷ���...
  if( m_lpRoom == NULL )
    return;
  // ��ȡ���������ʦ�����߶��� => �������ߣ�ֱ�ӷ���...
  CTeacher * lpTeacher = m_lpRoom->GetTeacherPusher();
  if( lpTeacher == NULL )
    return;
  // ��ȡ��ʦ�������ڷ������������Ƶ����Ƶ���ζ��ж���...
  circlebuf & cur_circle = bIsAudio ? lpTeacher->GetAudioCircle() : lpTeacher->GetVideoCircle();
  // ������ζ���Ϊ�գ�ֱ�ӷ���...
  if( cur_circle.size <= 0 )
    return;
  // ���ҵ����ζ�������ǰ�����ݰ���ͷָ�� => ��С���...
  rtp_hdr_t * lpFrontHeader = NULL;
  rtp_hdr_t * lpSendHeader = NULL;
  int nSendPos = 0, nSendSize = 0;
  /////////////////////////////////////////////////////////////////////////////////////////////////
  // ע�⣺ǧ�����ڻ��ζ��е��н���ָ���������start_pos > end_posʱ�����ܻ���Խ�����...
  // ���ԣ�һ��Ҫ�ýӿڶ�ȡ���������ݰ�֮���ٽ��в����������ָ�룬һ�������ػ����ͻ����...
  /////////////////////////////////////////////////////////////////////////////////////////////////
  const int nPerPackSize = DEF_MTU_SIZE + sizeof(rtp_hdr_t);
  static char szPacketBuffer[nPerPackSize] = {0};
  circlebuf_peek_front(&cur_circle, szPacketBuffer, nPerPackSize);
  lpFrontHeader = (rtp_hdr_t*)szPacketBuffer;
  // ���Ҫ��������ݰ���ű���С��Ż�ҪС => û���ҵ���ֱ�ӷ���...
  if( rtpLose.lose_seq < lpFrontHeader->seq ) {
    log_trace("[Student-Looker] Supply Error => lose: %u, min: %u, Type: %d", rtpLose.lose_seq, lpFrontHeader->seq, rtpLose.lose_type);
    return;
  }
  assert( rtpLose.lose_seq >= lpFrontHeader->seq );
  // ע�⣺���ζ��е��е����к�һ����������...
  // ����֮�����Ҫ�������ݰ���ͷָ��λ��...
  nSendPos = (rtpLose.lose_seq - lpFrontHeader->seq) * nPerPackSize;
  // �������λ�ô��ڻ���ڻ��ζ��г��� => ����Խ��...
  if( nSendPos >= cur_circle.size ) {
    log_trace("[Student-Looker] Supply Error => Position Excessed");
    return;
  }
  ////////////////////////////////////////////////////////////////////////////////////////////////////
  // ע�⣺�����ü򵥵�ָ����������ζ��п��ܻ�ػ��������ýӿ� => ��ָ�����λ�ÿ���ָ����������...
  // ��ȡ��Ҫ�������ݰ��İ�ͷλ�ú���Ч���ݳ���...
  ////////////////////////////////////////////////////////////////////////////////////////////////////
  memset(szPacketBuffer, 0, nPerPackSize);
  circlebuf_read(&cur_circle, nSendPos, szPacketBuffer, nPerPackSize);
  lpSendHeader = (rtp_hdr_t*)szPacketBuffer;
  // ����ҵ������λ�ò��� �� ���������Ҫ���Ķ���...
  if((lpSendHeader->pt == PT_TAG_LOSE) || (lpSendHeader->seq != rtpLose.lose_seq)) {
    log_trace("[Student-Looker] Supply Error => Seq: %u, Find: %u, Type: %d", rtpLose.lose_seq, lpSendHeader->seq, lpSendHeader->pt);
    return;
  }
  // ��ȡ��Ч������������ => ��ͷ + ����...
  nSendSize = sizeof(rtp_hdr_t) + lpSendHeader->psize;
  // ��ȡ��Ҫ����ر�����Ϣ...
  uint32_t nHostAddr = this->GetHostAddr();
  uint16_t nHostPort = this->GetHostPort();
  int listen_fd = GetApp()->GetListenFD();
  if( listen_fd <= 0 || nHostAddr <= 0 || nHostPort <= 0 )
    return;
  // ���챾�����Լ��Ľ��յ�ַ...
  sockaddr_in addrStudent = {0};
  addrStudent.sin_family = AF_INET;
  addrStudent.sin_port = htons(nHostPort);
  addrStudent.sin_addr.s_addr = htonl(nHostAddr);
  // �ظ�ѧ�������� => ����ͷ�Ѿ��յ�����Ҫ�ٷ�����ͷ������...
  if( sendto(listen_fd, (void*)lpSendHeader, nSendSize, 0, (sockaddr*)&addrStudent, sizeof(addrStudent)) < 0 ) {
    log_trace("sendto error(code:%d, %s)", errno, strerror(errno));
    return;
  }
  // ��ӡ�Ѿ����Ͳ�����Ϣ...
  log_trace("[Student-Looker Supply Send => Seq: %u, TS: %u, Slice: %d, Type: %d", lpSendHeader->seq, lpSendHeader->ts, lpSendHeader->psize, lpSendHeader->pt);
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
  // ���ͳɹ�...
  return true;
}

bool CStudent::doTagReady(char * lpBuffer, int inBufSize)
{
  // ֻ��ѧ�������߲Żᴦ��׼����������...
  if( this->GetIdTag() != ID_TAG_PUSHER )
    return false;
  // ѧ�������� => �ظ���ʦ�ۿ��ߣ��Ѿ��յ�׼�����������Ҫ�ٷ���׼������������...
  assert( this->GetIdTag() == ID_TAG_PUSHER );
  // ���Լ��Ĵ�͸��ַ���µ�׼�������ṹ�嵱��...
  rtp_ready_t * lpPushReady = (rtp_ready_t*)lpBuffer;
  lpPushReady->recvPort = this->GetHostPort();
  lpPushReady->recvAddr = this->GetHostAddr();
  // �ظ���ʦ�ۿ��ߣ��Ѿ��յ�׼�����������Ҫ�ٷ���׼������������...
  return this->doTransferToTeacherLooker(lpBuffer, inBufSize);
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
  // ���ͳɹ�...
  return true;
}

bool CStudent::doTransferToStudentLooker(char * lpBuffer, int inBufSize)
{
  // �ж�����Ļ������Ƿ���Ч...
  if( lpBuffer == NULL || inBufSize <= 0 )
    return false;
  // ����ֻ�������ѧ���ۿ��ߵ�����ת��...
  if( this->GetIdTag() != ID_TAG_LOOKER )
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
  // ��������Ϣת����ѧ���ۿ��߶���...
  if( sendto(listen_fd, lpBuffer, inBufSize, 0, (sockaddr*)&addrStudent, sizeof(addrStudent)) < 0 ) {
    log_trace("sendto error(code:%d, %s)", errno, strerror(errno));
    return false;
  }
  // ���ͳɹ�...
  return true; 
}