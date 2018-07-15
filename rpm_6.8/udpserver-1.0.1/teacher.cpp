
#include "app.h"
#include "room.h"
#include "student.h"
#include "teacher.h"

CTeacher::CTeacher(uint8_t tmTag, uint8_t idTag, uint32_t inHostAddr, uint16_t inHostPort)
  : CNetwork(tmTag, idTag, inHostAddr, inHostPort)
  , m_server_rtt_var_ms(-1)
  , m_server_rtt_ms(-1)
{
  // ��ʼ������ͷ��̽�������...
  m_strSeqHeader.clear();
  memset(&m_server_rtp_detect, 0, sizeof(rtp_detect_t));
	// ��ʼ������������Ƶ���ζ���...
	circlebuf_init(&m_audio_circle);
	circlebuf_init(&m_video_circle);
  // ����������ˣ�Ԥ���价�ζ��пռ�...
  if( this->GetIdTag() == ID_TAG_PUSHER ) {
    circlebuf_reserve(&m_audio_circle, 512 * 1024);
    circlebuf_reserve(&m_video_circle, 512 * 1024);
  }
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
	// �ͷ�����������Ƶ���ζ��пռ�...
	circlebuf_free(&m_audio_circle);
	circlebuf_free(&m_video_circle);
  // ����������ˣ����Լ��Ӳ������е���ɾ����...
  if( this->GetIdTag() == ID_TAG_PUSHER ) {
    GetApp()->doDelSupplyForTeacher(this);
  }
  // ��ӡ��ʦ�����ڵķ�����Ϣ...
  if( m_lpRoom != NULL ) {
    m_lpRoom->doDumpRoomInfo();
  }
}

bool CTeacher::doServerSendDetect()
{
  // ֻ����ʦ�����ˣ��������Ż���������̽������...
  if( this->GetIdTag() != ID_TAG_PUSHER )
    return false;
	// �������µ�ӵ������ => ɾ��ָ������ʱ���֮ǰ������Ƶ���ݰ�...
	this->doCalcAVJamStatus();
  // ���̽������� => ��������������...
  m_server_rtp_detect.tm     = TM_TAG_SERVER;
  m_server_rtp_detect.id     = ID_TAG_SERVER;
  m_server_rtp_detect.pt     = PT_TAG_DETECT;
  m_server_rtp_detect.tsSrc  = (uint32_t)(os_gettime_ns() / 1000000);
  m_server_rtp_detect.dtDir  = DT_TO_SERVER;
  m_server_rtp_detect.dtNum += 1;
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
  // �ظ���ʦ������ => ̽������ԭ��������Ϣ...
  if( sendto(listen_fd, &m_server_rtp_detect, sizeof(m_server_rtp_detect), 0, (sockaddr*)&addrTeacher, sizeof(addrTeacher)) < 0 ) {
    log_trace("sendto error(code:%d, %s)", errno, strerror(errno));
    return false;
  }
  // ���ͳɹ�...
  return true;
}

void CTeacher::doCalcAVJamStatus()
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
	// ��ȡ�ڴ�����ݰ������ݣ���ȡ���ʱ���...
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
		if (cur_buf_ms < 3000)
			break;
		assert(cur_buf_ms >= 3000);
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
	log_trace("[Teacher-Pusher] Video Jam => MinSeq: %u, MaxSeq: %u, Circle: %d", min_seq, max_seq, cur_circle.size/nPerPackSize);
	// ɾ����Ƶ���ʱ������ݰ� => �������ʱ���֮ǰ���������ݰ�����ɾ��...
	this->doEarseAudioByPTS(min_ts);
}
//
// ɾ����Ƶ���ʱ������ݰ�...
void CTeacher::doEarseAudioByPTS(uint32_t inTimeStamp)
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
	log_trace("[Teacher-Pusher] Audio Jam => MinSeq: %u, MaxSeq: %u, Circle: %d", min_seq, max_seq, cur_circle.size/nPerPackSize);
}
//
// ������С��Ű������ù��Ƿ�����Ч����...
uint32_t CTeacher::doCalcMinSeq(bool bIsAudio)
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

bool CTeacher::doTagDetect(char * lpBuffer, int inBufSize)
{
  // ��ʦ�ۿ��� => ������̽�����ת����ѧ��������...
  // ע�⣺������̽��� => ��ʦ�ۿ����Լ�̽�� + ��ʦ�ۿ���ת��ѧ��������̽��...
  if( this->GetIdTag() == ID_TAG_LOOKER ) {
    return this->doTransferToStudentPusher(lpBuffer, inBufSize);
  }
  // ��ʦ������ => ���յ�����̽�ⷴ���� => ��ʦ�������Լ� �� ������...
  // ע�⣺��Ҫͨ������̽������жϷ����ߣ�������ͬ�Ĳ���...
  assert( this->GetIdTag() == ID_TAG_PUSHER );
  // ͨ����һ���ֽڵĵ�2λ���ж��ն�����...
  uint8_t tmTag = lpBuffer[0] & 0x03;
  // ��ȡ��һ���ֽڵ���2λ���õ��ն����...
  uint8_t idTag = (lpBuffer[0] >> 2) & 0x03;
  // ��ȡ��һ���ֽڵĸ�4λ���õ����ݰ�����...
  uint8_t ptTag = (lpBuffer[0] >> 4) & 0x0F;
  // ����� ��ʦ������ �Լ�������̽�����ԭ����������ʦ������...
  if( tmTag == TM_TAG_TEACHER && idTag == ID_TAG_PUSHER ) {
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
    // �ظ���ʦ������ => ̽������ԭ��������Ϣ...
    if( sendto(listen_fd, lpBuffer, inBufSize, 0, (sockaddr*)&addrTeacher, sizeof(addrTeacher)) < 0 ) {
      log_trace("sendto error(code:%d, %s)", errno, strerror(errno));
      return false;
    }
    // ���ͳɹ�...
    return true;
  }
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
		log_debug("[Teacher-Pusher] Recv Detect => dtNum: %d, rtt: %d ms, rtt_var: %d ms", rtpDetect.dtNum, m_server_rtt_ms, m_server_rtt_var_ms);    
  }
  return true;
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
  // ���ͳɹ�...
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
  // ���ͳɹ�...
  return true;
}

bool CTeacher::doTagDelete(char * lpBuffer, int inBufSize)
{
  // ע�⣺ɾ�������Ѿ���CApp::doTagDelete()�����ش�����...
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
  // ֻ����ʦ�ۿ��߲Żᴦ��׼����������...
  if( this->GetIdTag() != ID_TAG_LOOKER )
    return false;
  // ��ʦ�ۿ��� => ת��ѧ�������ߣ��Ѿ��յ�����ͷ����ۿ����Ѿ�׼������...
  assert( this->GetIdTag() == ID_TAG_LOOKER );
  // ���Լ��Ĵ�͸��ַ���µ�׼�������ṹ�嵱��...
  rtp_ready_t * lpLookReady = (rtp_ready_t*)lpBuffer;
  lpLookReady->recvPort = this->GetHostPort();
  lpLookReady->recvAddr = this->GetHostAddr();
  // ת��ѧ�������ߣ��Ѿ��յ�����ͷ����ۿ����Ѿ�׼������...
  return this->doTransferToStudentPusher(lpBuffer, inBufSize);
}

bool CTeacher::doTagAudio(char * lpBuffer, int inBufSize)
{
  // ֻ����ʦ�����˲Żᴦ����Ƶ������...
  if( this->GetIdTag() != ID_TAG_PUSHER )
    return false;
  // ����Ƶ���ݰ���������...
  this->doTagAVPackProcess(lpBuffer, inBufSize);
  // ת����Ƶ���ݰ������е�ѧ���ۿ���...
  return this->doTransferToStudentLooker(lpBuffer, inBufSize);
}

bool CTeacher::doTagVideo(char * lpBuffer, int inBufSize)
{
  // ֻ����ʦ�����˲Żᴦ����Ƶ������...
  if( this->GetIdTag() != ID_TAG_PUSHER )
    return false;
  // ����Ƶ���ݰ���������...
  this->doTagAVPackProcess(lpBuffer, inBufSize);
  // ת����Ƶ���ݰ������е�ѧ���ۿ���...
  return this->doTransferToStudentLooker(lpBuffer, inBufSize);
}

void CTeacher::doTagAVPackProcess(char * lpBuffer, int inBufSize)
{
	// �ж��������ݰ�����Ч�� => ����С�����ݰ���ͷ�ṹ����...
	const int nPerPackSize = DEF_MTU_SIZE + sizeof(rtp_hdr_t);
	if( lpBuffer == NULL || inBufSize < sizeof(rtp_hdr_t) || inBufSize > nPerPackSize ) {
		log_trace("[Teacher-Pusher] Error => RecvLen: %d, Max: %d", inBufSize, nPerPackSize);
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
	// ���ִ�����󣬶������������ӡ������Ϣ...
	if( inBufSize != nDataSize || nZeroSize < 0 ) {
		log_trace("[Teacher-Pusher] Error => RecvLen: %d, DataSize: %d, ZeroSize: %d", inBufSize, nDataSize, nZeroSize);
		return;
	}
	// ����Ƶʹ�ò�ͬ�Ĵ������ͱ���...
	circlebuf & cur_circle = (pt_tag == PT_TAG_AUDIO) ? m_audio_circle : m_video_circle;
	// ���ȣ�����ǰ�����кŴӶ������е���ɾ��...
	this->doEraseLoseSeq(pt_tag, new_id);
	//////////////////////////////////////////////////////////////////////////////////////////////////
	// ע�⣺ÿ�����ζ����е����ݰ���С��һ���� => rtp_hdr_t + slice + Zero => 12 + 800 => 812
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
		//log_trace("[Teacher-Pusher] Max Seq: %u, Cricle: Zero", new_id);
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
		//log_trace("[Teacher-Pusher] Max Seq: %u, Circle: %d", new_id, cur_circle.size/nPerPackSize-1);
		return;
	}
	// ����Ƕ�����Ĳ���� => max_id > new_id
	if( max_id > new_id ) {
		// �����С��Ŵ��ڶ������ => ��ӡ����ֱ�Ӷ�����������...
		if( min_id > new_id ) {
			log_trace("[Teacher-Pusher] Supply Discard => Seq: %u, Min-Max: [%u, %u], Type: %d", new_id, min_id, max_id, pt_tag);
			return;
		}
		// ��С��Ų��ܱȶ������С...
		assert( min_id <= new_id );
		// ���㻺��������λ��...
		uint32_t nPosition = (new_id - min_id) * nPerPackSize;
		// ����ȡ���������ݸ��µ�ָ��λ��...
		circlebuf_place(&cur_circle, nPosition, lpBuffer, inBufSize);
		// ��ӡ�������Ϣ...
		log_trace("[Teacher-Pusher] Supply Success => Seq: %u, Min-Max: [%u, %u], Type: %d", new_id, min_id, max_id, pt_tag);
		return;
	}
	// ���������δ֪������ӡ��Ϣ...
	log_trace("[Teacher-Pusher] Supply Unknown => Seq: %u, Slice: %d, Min-Max: [%u, %u], Type: %d", new_id, lpNewHeader->psize, min_id, max_id, pt_tag);
}
//
// �鿴��ǰ���Ƿ���Ҫ�Ӷ���������ɾ��...
void CTeacher::doEraseLoseSeq(uint8_t inPType, uint32_t inSeqID)
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
	log_trace("[Teacher-Pusher] Supply Erase => LoseSeq: %u, ResendCount: %u, LoseSize: %u, Type: %d", inSeqID, nResendCount, theMapLose.size(), inPType);
}
//
// ����ʧ���ݰ�Ԥ�����ζ��л���ռ�...
void CTeacher::doFillLosePack(uint8_t inPType, uint32_t nStartLoseID, uint32_t nEndLoseID)
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
		log_trace("[Teacher-Pusher] Lose Seq: %u, LoseSize: %u, Type: %d", sup_id, theMapLose.size(), inPType);
		// �ۼӵ�ǰ�������к�...
		++sup_id;
	}
  // ���Լ����뵽���������б���...
  GetApp()->doAddSupplyForTeacher(this);
}

int CTeacher::doServerSendSupply()
{
  // -1 => ����Ƶ��û�в���...
  //  0 => �в���������������ʱ��...
  //  1 => �в������Ѿ����Ͳ�������...
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

int CTeacher::doSendSupplyCmd(bool bIsAudio)
{
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
      log_trace("[Teacher-Pusher] Supply Discard => LoseSeq: %u, MinSeq: %u, Audio: %d", rtpLose.lose_seq, min_id, bIsAudio);
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
  // ��ȡ��Ҫ����ر�����Ϣ...
  int listen_fd = GetApp()->GetListenFD();
  uint32_t nHostAddr = this->GetHostAddr();
  uint16_t nHostPort = this->GetHostPort();
  if( listen_fd <= 0 || nHostAddr <= 0 || nHostPort <= 0 )
    return 0;
  // ����ۿ��ߵĽ��յ�ַ...
	sockaddr_in addrTeacher = {0};
	addrTeacher.sin_family = AF_INET;
	addrTeacher.sin_port = htons(nHostPort);
	addrTeacher.sin_addr.s_addr = htonl(nHostAddr);
  // ������ͷͨ���������ת������ʦ�ۿ���...
  if( sendto(listen_fd, szPacketBuffer, nDataSize, 0, (sockaddr*)&addrTeacher, sizeof(addrTeacher)) < 0 ) {
    log_trace("sendto error(code:%d, %s)", errno, strerror(errno));
    return 0;
  }
	// ��ӡ�ѷ��Ͳ�������...
	log_trace("[Teacher-Pusher] Supply Send => Dir: %d, Count: %d, Audio: %d", DT_TO_SERVER, rtpSupply.suSize/sizeof(uint32_t), bIsAudio);
  // �ɹ����Ͳ����������1...
  return 1;
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
  // ת���������ݰ������з������ѧ�����߹ۿ���...
  return m_lpRoom->doTransferToStudentLooker(lpBuffer, inBufSize);
}