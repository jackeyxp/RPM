
#include "app.h"
#include "room.h"
#include "student.h"
#include "teacher.h"
#include "udpthread.h"

CStudent::CStudent(CUDPThread * lpUDPThread, uint8_t tmTag, uint8_t idTag, uint32_t inHostAddr, uint16_t inHostPort)
  : CNetwork(tmTag, idTag, inHostAddr, inHostPort)
  , m_nTCPRoleType(kRoleWanRecv)
  , m_lpUDPThread(lpUDPThread)
  , m_server_rtt_var_ms(-1)
  , m_server_rtt_ms(-1)
  , m_bIsCanDetect(true)
{
  assert(m_lpUDPThread != NULL);
  // ��ʼ������ͷ��̽�������...
  m_strSeqHeader.clear();
  memset(&m_server_rtp_detect, 0, sizeof(rtp_detect_t));
  // ��ʼ������������Ƶ���ζ���...
  circlebuf_init(&m_audio_circle);
  circlebuf_init(&m_video_circle);
  // �����ѧ�������ˣ�Ԥ���价�ζ��пռ�...
  if( this->GetIdTag() == ID_TAG_PUSHER ) {
    circlebuf_reserve(&m_audio_circle, 64 * 1024);
    circlebuf_reserve(&m_video_circle, 64 * 1024);
  }
  // ��ӡѧ���˱�������Ϣ...
  log_trace("[UDP-%s-%s-Create] HostPort: %d", get_tm_tag(tmTag), get_id_tag(idTag), inHostPort);
}

CStudent::~CStudent()
{
  // ��ӡѧ���˱�ɾ����Ϣ...
  log_trace("[UDP-%s-%s-Delete] HostPort: %d, LiveID: %d", 
            get_tm_tag(this->GetTmTag()), get_id_tag(this->GetIdTag()),
            this->GetHostPort(), this->GetDBCameraID());
  // �ڷ�����ע����ѧ���˶���...
  if( m_lpRoom != NULL ) {
    m_lpRoom->doDeleteStudent(this);
  }
  // �ͷ�����������Ƶ���ζ��пռ�...
  circlebuf_free(&m_audio_circle);
  circlebuf_free(&m_video_circle);
  // ����������ˣ����Լ��Ӳ������е���ɾ����...
  if( this->GetIdTag() == ID_TAG_PUSHER ) {
    m_lpUDPThread->doDelSupplyForPusher(this);
  }
  // ����ǹۿ��ˣ����Լ��Ӷ������е���ɾ����...
  if( this->GetIdTag() == ID_TAG_LOOKER ) {
    m_lpUDPThread->doDelLoseForLooker(this);
  }
  // ��ӡѧ�������ڵķ�����Ϣ...
  if( m_lpRoom != NULL ) {
    m_lpRoom->doDumpRoomInfo();
  }
}

bool CStudent::doServerSendDetect()
{
  // ֻ��ѧ�������ˣ��������Ż���������̽������...
  if( this->GetIdTag() != ID_TAG_PUSHER )
    return false;
  // �������µ�ӵ������ => ɾ��ָ������ʱ���֮ǰ������Ƶ���ݰ�...
  this->doCalcAVJamStatus();
  // ע�⣺����̽��������˻��ۻ����棬ֱ������Զ�ɾ��...
  // ����в��ܽ���̽���־��ֱ�ӷ���...
  if( !m_bIsCanDetect )
    return false;
  // ���̽������� => ��������������...
  m_server_rtp_detect.tm     = TM_TAG_SERVER;
  m_server_rtp_detect.id     = ID_TAG_SERVER;
  m_server_rtp_detect.pt     = PT_TAG_DETECT;
  m_server_rtp_detect.tsSrc  = (uint32_t)(os_gettime_ns() / 1000000);
  m_server_rtp_detect.dtDir  = DT_TO_SERVER;
  m_server_rtp_detect.dtNum += 1;
  // �������յ�����Ƶ�����������...
  m_server_rtp_detect.maxAConSeq = this->doCalcMaxConSeq(true);
  m_server_rtp_detect.maxVConSeq = this->doCalcMaxConSeq(false);
  // ���������Ч���ۼӷ�����������...
  int nFlowSize = sizeof(m_server_rtp_detect);
  if (m_lpRoom != NULL) { m_lpRoom->doAddDownFlowByte(nFlowSize); }
  // ���¹����̽���ת������ǰ����...
  return this->doTransferToFrom((char*)&m_server_rtp_detect, nFlowSize);
}

// ѧ�������� => �����ѽ��յ��������������Ű�...
uint32_t CStudent::doCalcMaxConSeq(bool bIsAudio)
{
  // �������ݰ����ͣ��ҵ��������ϡ����ζ��С���󲥷Ű�...
  GM_MapLose & theMapLose = bIsAudio ? m_AudioMapLose : m_VideoMapLose;
  circlebuf  & cur_circle = bIsAudio ? m_audio_circle : m_video_circle;
  // ���������ļ��� => ������С������ - 1
  if( theMapLose.size() > 0 ) {
    return (theMapLose.begin()->first - 1);
  }
  // û�ж��� => ���ζ���Ϊ�� => ����0...
  if( cur_circle.size <= 0 )
    return 0;
  // û�ж��� => ���յ��������� => ���ζ�����������к� - 1...
  const int nPerPackSize = DEF_MTU_SIZE + sizeof(rtp_hdr_t);
  static char szPacketBuffer[nPerPackSize] = {0};
  circlebuf_peek_back(&cur_circle, szPacketBuffer, nPerPackSize);
  rtp_hdr_t * lpMaxHeader = (rtp_hdr_t*)szPacketBuffer;
  return (lpMaxHeader->seq - 1);
}

void CStudent::doCalcAVJamStatus()
{
  // ��Ƶ���ζ���Ϊ�գ�û��ӵ����ֱ�ӷ���...
  if( m_video_circle.size <= 0 )
    return;
  // �������ζ��У�ɾ�����г���n��Ļ������ݰ� => �����Ƿ��ǹؼ�֡����������ֻ��Ϊ����������...
  const int nPerPackSize = DEF_MTU_SIZE + sizeof(rtp_hdr_t);
  static char szPacketBuffer[nPerPackSize] = {0};
  circlebuf & cur_circle = m_video_circle;
  rtp_hdr_t * lpCurHeader = NULL;
  uint32_t    min_ts = 0, min_seq = 0;
  uint32_t    max_ts = 0, max_seq = 0;
  // ��ȡ�������ݰ������ݣ���ȡ���ʱ���...
  circlebuf_peek_back(&cur_circle, szPacketBuffer, nPerPackSize);
  lpCurHeader = (rtp_hdr_t*)szPacketBuffer;
  max_seq = lpCurHeader->seq;
  max_ts = lpCurHeader->ts;
  // �������ζ��У��鿴�Ƿ�����Ҫɾ�������ݰ�...
  while ( cur_circle.size > 0 ) {
    // ��ȡ��һ�����ݰ������ݣ���ȡ��Сʱ���...
    circlebuf_peek_front(&cur_circle, szPacketBuffer, nPerPackSize);
    lpCurHeader = (rtp_hdr_t*)szPacketBuffer;
    // ���㻷�ζ��е����ܵĻ���ʱ��...
    uint32_t cur_buf_ms = max_ts - lpCurHeader->ts;
    // ����ܻ���ʱ�䲻����n�룬�жϲ���...
    if (cur_buf_ms < 5000)
      break;
    assert(cur_buf_ms >= 5000);
    // ����ɾ����ʱ��㣬����Ƶ�ο�...
    min_ts = lpCurHeader->ts;
    min_seq = lpCurHeader->seq;
    // ����ܻ���ʱ�䳬��n�룬ɾ����С���ݰ�������Ѱ��...
    circlebuf_pop_front(&cur_circle, NULL, nPerPackSize);
  }
  // ���û�з���ɾ����ֱ�ӷ���...
  if (min_ts <= 0 || min_seq <= 0 )
    return;
  // ��ӡ����ӵ����� => ������Ƶ�����ӵ�����...
  //log_trace("[%s-%s] Video Jam => MinSeq: %u, MaxSeq: %u, Circle: %d",
  //          get_tm_tag(this->GetTmTag()), get_id_tag(this->GetIdTag()),
  //          min_seq, max_seq, cur_circle.size/nPerPackSize);
  // ɾ����Ƶ���ʱ������ݰ� => �������ʱ���֮ǰ���������ݰ�����ɾ��...
  this->doEarseAudioByPTS(min_ts);
}
//
// ɾ����Ƶ���ʱ������ݰ�...
void CStudent::doEarseAudioByPTS(uint32_t inTimeStamp)
{
  // ��Ƶ���ζ���Ϊ�գ�ֱ�ӷ���...
  if (m_audio_circle.size <= 0)
    return;
  const int nPerPackSize = DEF_MTU_SIZE + sizeof(rtp_hdr_t);
  static char szPacketBuffer[nPerPackSize] = { 0 };
  circlebuf & cur_circle = m_audio_circle;
  rtp_hdr_t * lpCurHeader = NULL;
  uint32_t    min_seq = 0, max_seq = 0;
  // ��ȡ�ڴ�����ݰ������ݣ���ȡ������к�...
  circlebuf_peek_back(&cur_circle, szPacketBuffer, nPerPackSize);
  lpCurHeader = (rtp_hdr_t*)szPacketBuffer;
  max_seq = lpCurHeader->seq;
  // ������Ƶ���ζ��У�ɾ������ʱ���С������ʱ��������ݰ�...
  while( cur_circle.size > 0 ) {
    // ��ȡ��һ�����ݰ������ݣ���ȡ��Сʱ���...
    circlebuf_peek_front(&cur_circle, szPacketBuffer, nPerPackSize);
    lpCurHeader = (rtp_hdr_t*)szPacketBuffer;
    // �����ǰ����ʱ�����������ʱ�����ɾ����ֹ...
    if (lpCurHeader->ts > inTimeStamp)
      break;
    // ɾ����ǰ��С���ݰ�������Ѱ��...
    min_seq = lpCurHeader->seq;
    assert(lpCurHeader->ts <= inTimeStamp);
    circlebuf_pop_front(&cur_circle, NULL, nPerPackSize);
  }
  // ��ӡ��Ƶӵ����Ϣ => ��ǰλ�ã��ѷ������ݰ� => ����֮����ǹۿ��˵���Ч�����ռ�...
  //log_trace("[%s-%s] Audio Jam => MinSeq: %u, MaxSeq: %u, Circle: %d",
  //          get_tm_tag(this->GetTmTag()), get_id_tag(this->GetIdTag()),
  //          min_seq, max_seq, cur_circle.size/nPerPackSize);
}

bool CStudent::doTagDetect(char * lpBuffer, int inBufSize)
{
  // ���������Ч���ۼӷ�����������...
  if (m_lpRoom != NULL) { m_lpRoom->doAddUpFlowByte(inBufSize); }
  // ѧ���ۿ��� => ֻ��һ��̽��� => ѧ���ۿ����Լ���̽���...
  // ѧ���ۿ��� => ��̽���ԭ�����ظ��Լ�����������������ʱ...
  if( this->GetIdTag() == ID_TAG_LOOKER ) {
    return this->doDetectForLooker(lpBuffer, inBufSize);
  }
  // ѧ�������� => ���յ�����̽�ⷴ���� => ѧ���������Լ� �� ������...
  // ע�⣺��Ҫͨ������̽������жϷ����ߣ�������ͬ�Ĳ���...
  assert( this->GetIdTag() == ID_TAG_PUSHER );
  // ͨ����һ���ֽڵĵ�2λ���ж��ն�����...
  uint8_t tmTag = lpBuffer[0] & 0x03;
  // ��ȡ��һ���ֽڵ���2λ���õ��ն����...
  uint8_t idTag = (lpBuffer[0] >> 2) & 0x03;
  // ��ȡ��һ���ֽڵĸ�4λ���õ����ݰ�����...
  uint8_t ptTag = (lpBuffer[0] >> 4) & 0x0F;
  // ����� ѧ�������� �Լ�������̽�����ԭ��������ѧ��������...
  if( tmTag == TM_TAG_STUDENT && idTag == ID_TAG_PUSHER ) {
    if (m_lpRoom != NULL) { m_lpRoom->doAddDownFlowByte(inBufSize); }
    return this->doTransferToFrom(lpBuffer, inBufSize);
  }
  // ֻ��ѧ�������ߣ��������Żᷢ������̽���...
  // ����� ������ ������̽���������������ʱ...
  if( tmTag == TM_TAG_SERVER && idTag == ID_TAG_SERVER ) {
    // ��ȡ�յ���̽�����ݰ�...
    rtp_detect_t rtpDetect = { 0 };
    memcpy(&rtpDetect, lpBuffer, sizeof(rtpDetect));
    // ��ǰʱ��ת���ɺ��룬����������ʱ => ��ǰʱ�� - ̽��ʱ��...
    uint32_t cur_time_ms = (uint32_t)(os_gettime_ns() / 1000000);
    int keep_rtt = cur_time_ms - rtpDetect.tsSrc;
    // ��ֹ����ͻ���ӳ�����, ��� TCP �� RTT ����˥�����㷨...
    if (m_server_rtt_ms < 0) { m_server_rtt_ms = keep_rtt; }
    else { m_server_rtt_ms = (7 * m_server_rtt_ms + keep_rtt) / 8; }
    // �������綶����ʱ���ֵ => RTT������ֵ...
    if (m_server_rtt_var_ms < 0) { m_server_rtt_var_ms = abs(m_server_rtt_ms - keep_rtt); }
    else { m_server_rtt_var_ms = (m_server_rtt_var_ms * 3 + abs(m_server_rtt_ms - keep_rtt)) / 4; }
    // ��ӡ̽���� => ̽����� | ������ʱ(����)...
    //log_debug("[%s-%s] Recv Detect => dtNum: %d, rtt: %d ms, rtt_var: %d ms",
    //          get_tm_tag(this->GetTmTag()), get_id_tag(this->GetIdTag()),
    //          rtpDetect.dtNum, m_server_rtt_ms, m_server_rtt_var_ms);    
  }
  return true;
}

// ������ʦ�ۿ����ڼ��������µĻ�������...
// ������С��Ű������ù��Ƿ�����Ч����...
uint32_t CStudent::doCalcMinSeq(bool bIsAudio)
{
  // ����Ƶʹ�ò�ͬ���кͱ���...
  circlebuf & cur_circle = bIsAudio ? m_audio_circle : m_video_circle;
  // ������ζ���Ϊ�գ�ֱ�ӷ���0...
  if( cur_circle.size <= 0 )
    return 0;
  // ��ȡ��һ�����ݰ������ݣ���ȡ��С�����...
  const int nPerPackSize = DEF_MTU_SIZE + sizeof(rtp_hdr_t);
  static char szPacketBuffer[nPerPackSize] = { 0 };
  circlebuf_peek_front(&cur_circle, szPacketBuffer, nPerPackSize);
  rtp_hdr_t * lpCurHeader = (rtp_hdr_t*)szPacketBuffer;
  return lpCurHeader->seq;
}

// ע�⣺����Ƶ��С��Ű����ù��Ƿ�����Ч����ֻ�򵥻�ȡ��С����...
bool CStudent::doDetectForLooker(char * lpBuffer, int inBufSize)
{
  // ���û�з��䣬ֱ�ӷ���...
  if( m_lpRoom == NULL )
    return false;
  // ��̽�⻺��ǿ��ת����̽��ṹ��...
  rtp_detect_t * lpDetect = (rtp_detect_t*)lpBuffer;
  // ��ȡ���������ʦ�����ߺ�ѧ��������...
  CTeacher * lpTeacher = m_lpRoom->GetTeacherPusher();
  CStudent * lpStudent = m_lpRoom->GetStudentPusher();
  // ���ѧ����������Ч�������չ��Ƶ����С����...
  if( lpStudent != NULL ) {
    lpDetect->maxExAConSeq = lpStudent->doCalcMinSeq(true);
  }
  // ����ʦ�����ߵ�ǰ��С������Ƶ���ݰ��Ÿ��µ�̽�������...
  if( lpTeacher != NULL ) {
    lpDetect->maxAConSeq = lpTeacher->doCalcMinSeq(true);
    lpDetect->maxVConSeq = lpTeacher->doCalcMinSeq(false);
  }
  // ���������Ч���ۼӷ�����������...
  if (m_lpRoom != NULL) { m_lpRoom->doAddDownFlowByte(inBufSize); }
  // �����º�����ݰ�������ѧ���ۿ��˶���...
  return this->doTransferToFrom(lpBuffer, inBufSize);
}

bool CStudent::doTagCreate(char * lpBuffer, int inBufSize)
{
  //////////////////////////////////////////////////////
  // ע�⣺ѧ�������ߺ�ѧ���ۿ��ߴ���ʽ���в�ͬ...
  //////////////////////////////////////////////////////
  bool bResult = false;
  // ���´�����������ݣ���������·��䣬���·������ѧ����...
  memcpy(&m_rtp_create, lpBuffer, sizeof(m_rtp_create));
  m_lpRoom = m_lpUDPThread->doCreateRoom(m_rtp_create.roomID);
  m_lpRoom->doCreateStudent(this);
  // ���������Ч���ۼӷ�����������...
  m_lpRoom->doAddUpFlowByte(inBufSize);
  // �ظ�ѧ�������� => �����Ѿ������ɹ�����Ҫ�ٷ�����������...
  if( this->GetIdTag() == ID_TAG_PUSHER ) {
    bResult = this->doCreateForPusher(lpBuffer, inBufSize);
  }
  // �ظ�ѧ���ۿ��� => ����ʦ�����˵�����ͷת����ѧ���ۿ��� => ����û��P2Pģʽ���ۿ��˲��÷���׼����������...
  if( this->GetIdTag() == ID_TAG_LOOKER ) {
    bResult = this->doCreateForLooker(lpBuffer, inBufSize);
    m_nTCPRoleType = GetApp()->GetTCPRoleType(m_rtp_create.tcpSock);
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
  // ���������Ч���ۼӷ�����������...
  if (m_lpRoom != NULL) { m_lpRoom->doAddDownFlowByte(sizeof(rtpHdr)); }
  // �ظ�ѧ�������� => �����Ѿ������ɹ�����Ҫ�ٷ�����������...
  return this->doTransferToFrom((char*)&rtpHdr, sizeof(rtpHdr));
}
//
// �ظ�ѧ���ۿ��� => ����ʦ�����˵�����ͷת����ѧ���ۿ��� => ����û��P2Pģʽ���ۿ��˲��÷���׼����������...
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
  // ���������Ч���ۼӷ�����������...
  if (m_lpRoom != NULL) { m_lpRoom->doAddDownFlowByte(strSeqHeader.size()); }
  // �ظ�ѧ���ۿ��� => ����ʦ�����˵�����ͷת����ѧ���ۿ���...
  return this->doTransferToFrom((char*)strSeqHeader.c_str(), strSeqHeader.size());
}

bool CStudent::doTagDelete(char * lpBuffer, int inBufSize)
{
  // ע�⣺ɾ�������Ѿ���CUDPThread::doTagDelete()�����ش�����...
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
  // ע�⣺����ֻ�ǽ���Ҫ��������ż��뵽�������е��У����߳�ȥ����...
  // ע�⣺ѧ���ۿ��˵Ĳ����ӷ������ϵ���ʦ�����ߵĻ����ȡ...
  rtp_supply_t rtpSupply = {0};
  int nHeadSize = sizeof(rtp_supply_t);
  memcpy(&rtpSupply, lpBuffer, nHeadSize);
  if( (rtpSupply.suSize <= 0) || ((nHeadSize + rtpSupply.suSize) != inBufSize) )
    return false;
  // ���������Ч���ۼӷ�����������...
  if (m_lpRoom != NULL) { m_lpRoom->doAddUpFlowByte(inBufSize); }
  // �������ݰ����ͣ��ҵ��������� => ������չ��Ƶ���� => ѧ����������Ƶ...
  GM_MapLose & theMapLose = ((rtpSupply.suType == PT_TAG_EX_AUDIO) ? m_Ex_AudioMapLose : 
                            ((rtpSupply.suType == PT_TAG_AUDIO) ? m_AudioMapLose : m_VideoMapLose));
  // ��ȡ��Ҫ���������кţ����뵽��Ӧ�Ķ������е���...
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
    // �鿴����������Ƿ���������(��ʦ������|ѧ��������)ҲҪ���İ�...
    // �������յ���������Զ�ת��������Ͳ��ò���...
    if( this->doIsPusherLose(rtpSupply.suType, nLoseSeq) )
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
  m_lpUDPThread->doAddLoseForLooker(this);
  // ��ӡ���յ���������...
  //log_debug("[%s-%s] Supply Recv => Count: %d, Type: %d",
  //          get_tm_tag(this->GetTmTag()), get_id_tag(this->GetIdTag()),
  //          rtpSupply.suSize / sizeof(int), rtpSupply.suType);
  return true;
}

// �鿴����������Ƿ���������(��ʦ������|ѧ��������)ҲҪ���İ�...
bool CStudent::doIsPusherLose(uint8_t inPType, uint32_t inLoseSeq)
{
  // ���û�з��䣬ֱ�ӷ���...
  if( m_lpRoom == NULL )
    return false;
  // ��ȡ���������ʦ�����ߺ�ѧ��������...
  CTeacher * lpTeacher = m_lpRoom->GetTeacherPusher();
  CStudent * lpStudent = m_lpRoom->GetStudentPusher();
  // �������չ��Ƶ��������Ҫ��ѧ���������в���...
  if( inPType == PT_TAG_EX_AUDIO ) {
    // ��ȡ�������ѧ�������߶��� => �������ߣ�ֱ�ӷ���...
    if( lpStudent == NULL ) return false;
    // ��ѧ���������в鿴�Ƿ��ڶ�Ӧ�Ĳ������е���...
    return lpStudent->doIsStudentPusherLose(true, inLoseSeq);
  }
  // �������ʦ����Ƶ��������Ҫ����ʦ�������в���...
  bool bIsAudio = ((inPType == PT_TAG_AUDIO) ? true : false);
  // ��ȡ���������ʦ�����߶��� => �������ߣ�ֱ�ӷ���...
  if( lpTeacher == NULL ) return false;
  // ����ʦ�������в鿴�Ƿ��ڶ�Ӧ�Ĳ������е���...
  return lpTeacher->doIsTeacherPusherLose(bIsAudio, inLoseSeq);
}

bool CStudent::doIsStudentPusherLose(bool bIsAudio, uint32_t inLoseSeq)
{
  GM_MapLose & theMapLose = bIsAudio ? m_AudioMapLose : m_VideoMapLose;
  return ((theMapLose.find(inLoseSeq) != theMapLose.end()) ? true : false);
}

bool CStudent::doTagHeader(char * lpBuffer, int inBufSize)
{
  // ֻ��ѧ�������˲Żᴦ������ͷ����...
  if( this->GetIdTag() != ID_TAG_PUSHER )
    return false;
  // ���������Ч���ۼӷ�����������...
  if (m_lpRoom != NULL) { m_lpRoom->doAddUpFlowByte(inBufSize); }
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
  // ���������Ч���ۼӷ�����������...
  if (m_lpRoom != NULL) { m_lpRoom->doAddDownFlowByte(sizeof(rtpHdr)); }
  // �ظ�ѧ�������� => ����ͷ�Ѿ��յ�����Ҫ�ٷ�����ͷ������...
  return this->doTransferToFrom((char*)&rtpHdr, sizeof(rtpHdr));
}

bool CStudent::doTagReady(char * lpBuffer, int inBufSize)
{
  // ����ȥ����P2Pģʽ�������ٴ���׼������������...
  return true;
  /*// ֻ��ѧ�������߲Żᴦ��׼����������...
  if( this->GetIdTag() != ID_TAG_PUSHER )
    return false;
  // ѧ�������� => �ظ���ʦ�ۿ��ߣ��Ѿ��յ�׼�����������Ҫ�ٷ���׼������������...
  assert( this->GetIdTag() == ID_TAG_PUSHER );
  // ���Լ��Ĵ�͸��ַ���µ�׼�������ṹ�嵱��...
  rtp_ready_t * lpPushReady = (rtp_ready_t*)lpBuffer;
  lpPushReady->recvPort = this->GetHostPort();
  lpPushReady->recvAddr = this->GetHostAddr();
  // �ظ���ʦ�ۿ��ߣ��Ѿ��յ�׼�����������Ҫ�ٷ���׼������������...
  return this->doTransferToTeacherLooker(lpBuffer, inBufSize);*/
}

bool CStudent::doTagAudio(char * lpBuffer, int inBufSize)
{
  // ֻ��ѧ�������˲Żᴦ����Ƶ������...
  if( this->GetIdTag() != ID_TAG_PUSHER )
    return false;
  // ���������Ч���ۼӷ�����������...
  if (m_lpRoom != NULL) { m_lpRoom->doAddUpFlowByte(inBufSize); }
  // ����Ƶ���ݰ���������...
  this->doTagAVPackProcess(lpBuffer, inBufSize);
  // ת����Ƶ���ݰ������������ʦ�ۿ��߶���...
  this->doTransferToTeacherLooker(lpBuffer, inBufSize);
  // ������������Ч��ֱ�ӷ���...
  if( m_lpRoom == NULL ) return false;
  // ת����Ƶ���ݰ���������ѧ���ۿ����б���Ҫ�Ų��������Լ��γɵĹۿ��߶���...
  // ע�⣺ת��ʱ��Ҫ����Ƶ���ݰ���������ļӹ����� => ��Ƶͷ��䱣���ֶ�...
  return m_lpRoom->doStudentPusherToStudentLooker(lpBuffer, inBufSize);
}

bool CStudent::doTagVideo(char * lpBuffer, int inBufSize)
{
  // ֻ��ѧ�������˲Żᴦ����Ƶ������...
  if( this->GetIdTag() != ID_TAG_PUSHER )
    return false;
  // ���������Ч���ۼӷ�����������...
  if (m_lpRoom != NULL) { m_lpRoom->doAddUpFlowByte(inBufSize); }
  // ����Ƶ���ݰ���������...
  this->doTagAVPackProcess(lpBuffer, inBufSize);
  // ת����Ƶ���ݰ������������ʦ�ۿ���...
  return this->doTransferToTeacherLooker(lpBuffer, inBufSize);
}

void CStudent::doTagAVPackProcess(char * lpBuffer, int inBufSize)
{
  const char * lpTMTag = get_tm_tag(this->GetTmTag());
  const char * lpIDTag = get_id_tag(this->GetIdTag());
  // �ж��������ݰ�����Ч�� => ����С�����ݰ���ͷ�ṹ����...
  const int nPerPackSize = DEF_MTU_SIZE + sizeof(rtp_hdr_t);
  if( lpBuffer == NULL || inBufSize < sizeof(rtp_hdr_t) || inBufSize > nPerPackSize ) {
  	log_trace("[%s-%s] Error => RecvLen: %d, Max: %d", lpTMTag, lpIDTag, inBufSize, nPerPackSize);
  	return;
  }
  // ����յ��Ļ��������Ȳ��� �� �����Ϊ������ֱ�Ӷ���...
  rtp_hdr_t * lpNewHeader = (rtp_hdr_t*)lpBuffer;
  int nDataSize = lpNewHeader->psize + sizeof(rtp_hdr_t);
  int nZeroSize = DEF_MTU_SIZE - lpNewHeader->psize;
  uint8_t  pt_tag = lpNewHeader->pt;
  uint32_t new_id = lpNewHeader->seq;
  uint32_t max_id = new_id;
  uint32_t min_id = new_id;
  // ��ӡ�����˷������ݵĵ�����Ϣ...
  //log_debug("[%s-%s] Size: %d, Type: %d, Seq: %u, TS: %u, pst: %d, ped: %d, Slice: %d, Zero: %d", lpTMTag, lpIDTag, inBufSize,
  //          lpNewHeader->pt, lpNewHeader->seq, lpNewHeader->ts, lpNewHeader->pst,
  //          lpNewHeader->ped, lpNewHeader->psize, nZeroSize);
  // ���ִ�����󣬶������������ӡ������Ϣ...
  if( inBufSize != nDataSize || nZeroSize < 0 ) {
  	log_trace("[%s-%s] Error => RecvLen: %d, DataSize: %d, ZeroSize: %d", lpTMTag, lpIDTag, inBufSize, nDataSize, nZeroSize);
  	return;
  }
  // ����Ƶʹ�ò�ͬ�Ĵ������ͱ���...
  circlebuf & cur_circle = (pt_tag == PT_TAG_AUDIO) ? m_audio_circle : m_video_circle;
  // ���ȣ�����ǰ�����кŴӶ������е���ɾ��...
  this->doEraseLoseSeq(pt_tag, new_id);
  //////////////////////////////////////////////////////////////////////////////////////////////////
  // ע�⣺ÿ�����ζ����е����ݰ���С��һ���� => rtp_hdr_t + slice + Zero
  //////////////////////////////////////////////////////////////////////////////////////////////////
  static char szPacketBuffer[nPerPackSize] = {0};
  // ������ζ���Ϊ�� => ��Ҫ�Զ�������ǰԤ�в����д���...
  if( cur_circle.size < nPerPackSize ) {
    // �µ���Ű�����󲥷Ű�֮���п�϶��˵���ж���...
    // ���������� => [0 + 1, new_id - 1]
    if( new_id > (0 + 1) ) {
    	this->doFillLosePack(pt_tag, 0 + 1, new_id - 1);
    }
    // ��������Ű�ֱ��׷�ӵ����ζ��е�����棬�������󲥷Ű�֮���п�϶���Ѿ���ǰ��Ĳ����в������...
    // �ȼ����ͷ����������...
    circlebuf_push_back(&cur_circle, lpBuffer, inBufSize);
    // �ټ�������������ݣ���֤�������Ǳ���һ��MTU��Ԫ��С...
    if( nZeroSize > 0 ) {
    	circlebuf_push_back_zero(&cur_circle, nZeroSize);
    }
    // ��ӡ��׷�ӵ���Ű� => ������û�ж�������Ҫ׷���������Ű�...
    //log_trace("[%s-%s] Max Seq: %u, Cricle: Zero", lpTMTag, lpIDTag, new_id);
    return;
  }
  // ���ζ���������Ҫ��һ�����ݰ�...
  assert( cur_circle.size >= nPerPackSize );
  // ��ȡ���ζ�������С���к�...
  circlebuf_peek_front(&cur_circle, szPacketBuffer, nPerPackSize);
  rtp_hdr_t * lpMinHeader = (rtp_hdr_t*)szPacketBuffer;
  min_id = lpMinHeader->seq;
  // ��ȡ���ζ�����������к�...
  circlebuf_peek_back(&cur_circle, szPacketBuffer, nPerPackSize);
  rtp_hdr_t * lpMaxHeader = (rtp_hdr_t*)szPacketBuffer;
  max_id = lpMaxHeader->seq;
	// ������������ => max_id + 1 < new_id
	// ���������� => [max_id + 1, new_id - 1];
	if( max_id + 1 < new_id ) {
		this->doFillLosePack(pt_tag, max_id + 1, new_id - 1);
	}
	// ����Ƕ�����������Ű������뻷�ζ��У�����...
	if( max_id + 1 <= new_id ) {
		// �ȼ����ͷ����������...
		circlebuf_push_back(&cur_circle, lpBuffer, inBufSize);
		// �ټ�������������ݣ���֤�������Ǳ���һ��MTU��Ԫ��С...
		if( nZeroSize > 0 ) {
			circlebuf_push_back_zero(&cur_circle, nZeroSize);
		}
		// ��ӡ�¼���������Ű�...
		//log_trace("[%s-%s] Max Seq: %u, Circle: %d", lpTMTag, lpIDTag, new_id, cur_circle.size/nPerPackSize-1);
		return;
	}
	// ����Ƕ�����Ĳ���� => max_id > new_id
	if( max_id > new_id ) {
		// �����С��Ŵ��ڶ������ => ��ӡ����ֱ�Ӷ�����������...
		if( min_id > new_id ) {
			//log_trace("[%s-%s] Supply Discard => Seq: %u, Min-Max: [%u, %u], Type: %d", lpTMTag, lpIDTag, new_id, min_id, max_id, pt_tag);
			return;
		}
		// ��С��Ų��ܱȶ������С...
		assert( min_id <= new_id );
		// ���㻺��������λ��...
		uint32_t nPosition = (new_id - min_id) * nPerPackSize;
		// ����ȡ���������ݸ��µ�ָ��λ��...
		circlebuf_place(&cur_circle, nPosition, lpBuffer, inBufSize);
		// ��ӡ�������Ϣ...
		//log_trace("[%s-%s] Supply Success => Seq: %u, Min-Max: [%u, %u], Type: %d", lpTMTag, lpIDTag, new_id, min_id, max_id, pt_tag);
		return;
	}
	// ���������δ֪������ӡ��Ϣ...
	log_trace("[%s-%s] Supply Unknown => Seq: %u, Slice: %d, Min-Max: [%u, %u], Type: %d",
             lpTMTag, lpIDTag, new_id, lpNewHeader->psize, min_id, max_id, pt_tag);
}
//
// �鿴��ǰ���Ƿ���Ҫ�Ӷ���������ɾ��...
void CStudent::doEraseLoseSeq(uint8_t inPType, uint32_t inSeqID)
{
	// �������ݰ����ͣ��ҵ���������...
	GM_MapLose & theMapLose = (inPType == PT_TAG_AUDIO) ? m_AudioMapLose : m_VideoMapLose;
	// ���û���ҵ�ָ�������кţ�ֱ�ӷ���...
	GM_MapLose::iterator itorItem = theMapLose.find(inSeqID);
	if( itorItem == theMapLose.end() )
		return;
	// ɾ����⵽�Ķ����ڵ�...
	rtp_lose_t & rtpLose = itorItem->second;
	uint32_t nResendCount = rtpLose.resend_count;
	theMapLose.erase(itorItem);
	// ��ӡ���յ��Ĳ�����Ϣ����ʣ�µ�δ��������...
	//log_trace("[%s-%s] Supply Erase => LoseSeq: %u, ResendCount: %u, LoseSize: %u, Type: %d",
  //          get_tm_tag(this->GetTmTag()), get_id_tag(this->GetIdTag()),
  //          inSeqID, nResendCount, theMapLose.size(), inPType);
}
//
// ����ʧ���ݰ�Ԥ�����ζ��л���ռ�...
void CStudent::doFillLosePack(uint8_t inPType, uint32_t nStartLoseID, uint32_t nEndLoseID)
{
	// �������ݰ����ͣ��ҵ���������...
	circlebuf & cur_circle = (inPType == PT_TAG_AUDIO) ? m_audio_circle : m_video_circle;
	GM_MapLose & theMapLose = (inPType == PT_TAG_AUDIO) ? m_AudioMapLose : m_VideoMapLose;
	// ��Ҫ�����綶��ʱ��������·ѡ�� => ֻ��һ����������·...
	int cur_rtt_var_ms = m_server_rtt_var_ms;
	// ׼�����ݰ��ṹ�岢���г�ʼ�� => �������������ó���ͬ���ط�ʱ��㣬���򣬻��������ǳ���Ĳ�������...
	uint32_t cur_time_ms = (uint32_t)(os_gettime_ns() / 1000000);
	uint32_t sup_id = nStartLoseID;
	rtp_hdr_t rtpDis = {0};
	rtpDis.pt = PT_TAG_LOSE;
	// ע�⣺�Ǳ����� => [nStartLoseID, nEndLoseID]
	while( sup_id <= nEndLoseID ) {
		// ����ǰ����Ԥ��������...
		rtpDis.seq = sup_id;
		circlebuf_push_back(&cur_circle, &rtpDis, sizeof(rtpDis));
		circlebuf_push_back_zero(&cur_circle, DEF_MTU_SIZE);
		// ��������ż��붪�����е��� => ����ʱ�̵�...
		rtp_lose_t rtpLose = {0};
		rtpLose.resend_count = 0;
		rtpLose.lose_seq = sup_id;
		rtpLose.lose_type = inPType;
		// ע�⣺����Ҫ���� ���綶��ʱ��� Ϊ��������� => ��û����ɵ�һ��̽��������Ҳ����Ϊ0�������ҷ���...
		// �ط�ʱ��� => cur_time + rtt_var => ����ʱ�ĵ�ǰʱ�� + ����ʱ�����綶��ʱ��� => ���ⲻ�Ƕ�����ֻ�����������...
		rtpLose.resend_time = cur_time_ms + max(cur_rtt_var_ms, MAX_SLEEP_MS);
		theMapLose[sup_id] = rtpLose;
		// ��ӡ�Ѷ�����Ϣ���������г���...
		//log_trace("[%s-%s] Lose Seq: %u, LoseSize: %u, Type: %d", get_tm_tag(this->GetTmTag()), get_id_tag(this->GetIdTag()), sup_id, theMapLose.size(), inPType);
		// �ۼӵ�ǰ�������к�...
		++sup_id;
	}
  // ���Լ����뵽���������б���...
  m_lpUDPThread->doAddSupplyForPusher(this);
}

// �����߲Ż��в�������...
int CStudent::doServerSendSupply()
{
  // -1 => ����Ƶ��û�в���...
  //  0 => �в���������������ʱ��...
  //  1 => �в������Ѿ����Ͳ�������...
  // ������������ߣ�ֱ�ӷ���û�в�������...
  if( this->GetIdTag() != ID_TAG_PUSHER )
    return -1;
  // ����Ƶ�������Ͳ�������...
  int nRetAudio = this->doSendSupplyCmd(true);
  int nRetVideo = this->doSendSupplyCmd(false);
  // �������Ƶ��С��0������-1...
  if( nRetAudio < 0 && nRetVideo < 0 )
    return -1;
  // ֻҪ��һ������0������1...
  if( nRetAudio > 0 || nRetVideo > 0 )
    return 1;
  // ������������0...
  return 0;
}

// �����߲Ż��в�������...
int CStudent::doSendSupplyCmd(bool bIsAudio)
{
  // -1 => û�в���...
  //  0 => �в���������������ʱ��...
  //  1 => �в������Ѿ����Ͳ�������...
  
  const char * lpTMTag = get_tm_tag(this->GetTmTag());
  const char * lpIDTag = get_id_tag(this->GetIdTag());
  
  // �������ݰ����ͣ��ҵ���������...
  circlebuf & cur_circle = bIsAudio ? m_audio_circle : m_video_circle;
  GM_MapLose & theMapLose = bIsAudio ? m_AudioMapLose : m_VideoMapLose;
  // ����������϶���Ϊ�գ�ֱ�ӷ���...
  if( theMapLose.size() <= 0 )
    return -1;
  assert( theMapLose.size() > 0 );
  // �������Ĳ���������...
  const int nHeadSize = sizeof(rtp_supply_t);
  const int nPerPackSize = DEF_MTU_SIZE + nHeadSize;
  static char szPacketBuffer[nPerPackSize] = {0};
  uint32_t min_id = 0;
  // ��ȡ���ζ�������С���к�...
  if( cur_circle.size > nPerPackSize ) {
    circlebuf_peek_front(&cur_circle, szPacketBuffer, nPerPackSize);
    rtp_hdr_t * lpMinHeader = (rtp_hdr_t*)szPacketBuffer;
    min_id = lpMinHeader->seq;
  }
  // ��ȡ����������������ݵ�ͷָ��...
  char * lpData = szPacketBuffer + nHeadSize;
  // ��ȡ��ǰʱ��ĺ���ֵ => С�ڻ���ڵ�ǰʱ��Ķ�������Ҫ֪ͨ���Ͷ��ٴη���...
  uint32_t cur_time_ms = (uint32_t)(os_gettime_ns() / 1000000);
  // ��Ҫ�����������ӳ�ֵ������·ѡ�� => ֻ��һ����������·...
  int cur_rtt_ms = m_server_rtt_ms;
  // ���ò�������Ϊ0 => ���¼�����Ҫ�����ĸ���...
  // ��Ҫ����Ϊ�ӷ����������Ĳ�������...
  rtp_supply_t rtpSupply = {0};
  rtpSupply.tm = TM_TAG_SERVER;
  rtpSupply.id = ID_TAG_SERVER;
  rtpSupply.pt = PT_TAG_SUPPLY;
  rtpSupply.suSize = 0;
  rtpSupply.suType = bIsAudio ? PT_TAG_AUDIO : PT_TAG_VIDEO;
  // �����������У��ҳ���Ҫ�����Ķ������к�...
  GM_MapLose::iterator itorItem = theMapLose.begin();
  while( itorItem != theMapLose.end() ) {
    rtp_lose_t & rtpLose = itorItem->second;
    // ���Ҫ���İ��ţ�����С���Ż�ҪС��ֱ�Ӷ������Ѿ�������...
    if( rtpLose.lose_seq < min_id ) {
      log_trace("[%s-%s] Supply Discard => LoseSeq: %u, MinSeq: %u, Audio: %d", lpTMTag, lpIDTag, rtpLose.lose_seq, min_id, bIsAudio);
      theMapLose.erase(itorItem++);
      continue;
    }
    // �����������Ч��Χ֮��...
    if( rtpLose.resend_time <= cur_time_ms ) {
      // ����������峬���趨�����ֵ������ѭ�� => ��ಹ��200��...
      if( (nHeadSize + rtpSupply.suSize) >= nPerPackSize )
        break;
      // �ۼӲ������Ⱥ�ָ�룬�����������к�...
      memcpy(lpData, &rtpLose.lose_seq, sizeof(uint32_t));
      rtpSupply.suSize += sizeof(uint32_t);
      lpData += sizeof(uint32_t);
      // �ۼ��ط�����...
      ++rtpLose.resend_count;
      // ע�⣺ͬʱ���͵Ĳ������´�Ҳͬʱ���ͣ������γɶ��ɢ�еĲ�������...
      // ע�⣺���һ������������ʱ��û���յ����������Ҫ�ٴη���������Ĳ�������...
      // ע�⣺����Ҫ���� ���綶��ʱ��� Ϊ��������� => ��û����ɵ�һ��̽��������Ҳ����Ϊ0�������ҷ���...
      // �����´��ش�ʱ��� => cur_time + rtt => ����ʱ�ĵ�ǰʱ�� + ���������ӳ�ֵ => ��Ҫ������·ѡ��...
      rtpLose.resend_time = cur_time_ms + max(cur_rtt_ms, MAX_SLEEP_MS);
      // ���������������1���´β�����Ҫ̫�죬׷��һ����Ϣ����..
      rtpLose.resend_time += ((rtpLose.resend_count > 1) ? MAX_SLEEP_MS : 0);
    }
    // �ۼӶ������Ӷ���...
    ++itorItem;
  }
  // ������������Ϊ�� => ����ʱ��δ��...
  if( rtpSupply.suSize <= 0 )
    return 0;
  // ���²�������ͷ���ݿ�...
  memcpy(szPacketBuffer, &rtpSupply, nHeadSize);
  // ����������岻Ϊ�գ��Ž��в��������...
  int nDataSize = nHeadSize + rtpSupply.suSize;
  // ��ӡ�ѷ��Ͳ�������...
  //log_debug("[%s-%s] Supply Send => Dir: %d, Count: %d, Audio: %d", lpTMTag, lpIDTag, DT_TO_SERVER, rtpSupply.suSize/sizeof(uint32_t), bIsAudio);
  // ���������Ч���ۼӷ�����������...
  if (m_lpRoom != NULL) { m_lpRoom->doAddDownFlowByte(nDataSize); }
   // ����������ת������ǰ��ʦ�����߶���...
  return this->doTransferToFrom(szPacketBuffer, nDataSize);
}

// �ۿ��߲Ż��ж�������...
bool CStudent::doServerSendLose()
{
  // ������ǹۿ��ߣ�ֱ�ӷ���û�ж�������...
  if( this->GetIdTag() != ID_TAG_LOOKER )
    return false;
  // ���͹ۿ�����Ҫ����Ƶ��ʧ���ݰ�...
  this->doSendLosePacket(PT_TAG_AUDIO);
  // ���͹ۿ�����Ҫ����Ƶ��ʧ���ݰ�...
  this->doSendLosePacket(PT_TAG_VIDEO);
  // ���͹ۿ�����Ҫ����չ��Ƶ��ʧ���ݰ�...
  this->doSendLosePacket(PT_TAG_EX_AUDIO);
  // �����Ƶ����Ƶ��û�ж������ݣ�����false...
  if( m_AudioMapLose.size() <= 0 && m_VideoMapLose.size() <= 0 && m_Ex_AudioMapLose.size() <= 0) {
    return false;
  }
  // ��Ƶ|��Ƶ|��չ��ƵֻҪ��һ�����в�����ţ�����true...
  return true;
}

// �ۿ��߲Ż��ж�������...
void CStudent::doSendLosePacket(uint8_t inPType)
{
  const char * lpTMTag = get_tm_tag(this->GetTmTag());
  const char * lpIDTag = get_id_tag(this->GetIdTag());

  // �������ݰ����ͣ��ҵ��������ϡ����ζ��� => ������չ��Ƶ���� => ѧ����������Ƶ...
  GM_MapLose & theMapLose = ((inPType == PT_TAG_EX_AUDIO) ? m_Ex_AudioMapLose : 
                            ((inPType == PT_TAG_AUDIO) ? m_AudioMapLose : m_VideoMapLose));
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
  // ��ȡ���������ʦ�����߶����ѧ�������߶���...
  CTeacher * lpTeacher = m_lpRoom->GetTeacherPusher();
  CStudent * lpStudent = m_lpRoom->GetStudentPusher();
  // �������չ��Ƶ��ѧ����������Ч��ֱ�ӷ���...
  if((inPType == PT_TAG_EX_AUDIO) && (lpStudent == NULL))
    return;
  // �������������Ƶ����ʦ��������Ч��ֱ�ӷ���...
  if((inPType != PT_TAG_EX_AUDIO) && (lpTeacher == NULL))
    return;
  // ��ȡ��ʦ�������ڷ������������Ƶ����Ƶ���ζ��ж���...
  circlebuf & cur_circle = ((inPType == PT_TAG_EX_AUDIO) ? lpStudent->GetAudioCircle() :
                           ((inPType == PT_TAG_AUDIO) ? lpTeacher->GetAudioCircle() : lpTeacher->GetVideoCircle()));
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
    log_trace("[%s-%s] Lose Error => lose: %u, min: %u, Type: %d", lpTMTag, lpIDTag, rtpLose.lose_seq, lpFrontHeader->seq, rtpLose.lose_type);
    return;
  }
  assert( rtpLose.lose_seq >= lpFrontHeader->seq );
  // ע�⣺���ζ��е��е����к�һ����������...
  // ����֮�����Ҫ�������ݰ���ͷָ��λ��...
  nSendPos = (rtpLose.lose_seq - lpFrontHeader->seq) * nPerPackSize;
  // �������λ�ô��ڻ���ڻ��ζ��г��� => ����Խ��...
  if( nSendPos >= cur_circle.size ) {
    log_trace("[%s-%s] Lose Error => Position Excessed", lpTMTag, lpIDTag);
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
    log_trace("[%s-%s] Lose Error => Seq: %u, Find: %u, Type: %d", lpTMTag, lpIDTag, rtpLose.lose_seq, lpSendHeader->seq, lpSendHeader->pt);
    return;
  }
  // ��ȡ��Ч������������ => ��ͷ + ����...
  nSendSize = sizeof(rtp_hdr_t) + lpSendHeader->psize;
  // ��ӡ�Ѿ����Ͳ�����Ϣ...
  //log_debug("[%s-%s] Lose Send => Seq: %u, TS: %u, Slice: %d, Type: %d",
  //          lpTMTag, lpIDTag, lpSendHeader->seq, lpSendHeader->ts,
  //          lpSendHeader->psize, lpSendHeader->pt);
  // ���������Ч���ۼӷ�����������...
  if (m_lpRoom != NULL) { m_lpRoom->doAddDownFlowByte(nSendSize); }
  // �ظ�ѧ���ۿ��� => ���Ͳ���������������...
  this->doTransferToFrom((char*)lpSendHeader, nSendSize);
}

// ѧ�����������ݰ���ת�������������ʦ�ۿ��߶���...
bool CStudent::doTransferToTeacherLooker(char * lpBuffer, int inBufSize)
{
  // ���û�з��䣬ֱ�ӷ���...
  if( m_lpRoom == NULL )
    return false;
  // ��ȡ���������ʦ�ۿ��߶��� => �޹ۿ��ߣ�ֱ�ӷ���...
  CTeacher * lpTeacher = m_lpRoom->GetTeacherLooker();
  if( lpTeacher == NULL )
    return false;
  // ���������Ч���ۼӷ�����������...
  if (m_lpRoom != NULL) { m_lpRoom->doAddDownFlowByte(inBufSize); }
  // �����������ת�������������ʦ�ۿ��߶���...
  return lpTeacher->doTransferToFrom(lpBuffer, inBufSize);
}
