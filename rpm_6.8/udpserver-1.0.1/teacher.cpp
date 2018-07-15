
#include "app.h"
#include "room.h"
#include "student.h"
#include "teacher.h"

CTeacher::CTeacher(uint8_t tmTag, uint8_t idTag, uint32_t inHostAddr, uint16_t inHostPort)
  : CNetwork(tmTag, idTag, inHostAddr, inHostPort)
  , m_server_rtt_var_ms(-1)
  , m_server_rtt_ms(-1)
{
  // 初始化序列头和探测命令包...
  m_strSeqHeader.clear();
  memset(&m_server_rtp_detect, 0, sizeof(rtp_detect_t));
	// 初始化推流端音视频环形队列...
	circlebuf_init(&m_audio_circle);
	circlebuf_init(&m_video_circle);
  // 如果是推流端，预分配环形队列空间...
  if( this->GetIdTag() == ID_TAG_PUSHER ) {
    circlebuf_reserve(&m_audio_circle, 512 * 1024);
    circlebuf_reserve(&m_video_circle, 512 * 1024);
  }
  // 打印老师端被创建信息...
  log_debug("[Teacher-Create] tmTag: %d, idTag: %d", tmTag, idTag);
}

CTeacher::~CTeacher()
{
  // 打印老师端被删除信息...
  log_debug("[Teacher-Delete] tmTag: %d, idTag: %d", this->GetTmTag(), this->GetIdTag());
  // 在房间中注销本老师端对象...
  if( m_lpRoom != NULL ) {
    m_lpRoom->doDeleteTeacher(this);
  }  
	// 释放推流端音视频环形队列空间...
	circlebuf_free(&m_audio_circle);
	circlebuf_free(&m_video_circle);
  // 如果是推流端，把自己从补包队列当中删除掉...
  if( this->GetIdTag() == ID_TAG_PUSHER ) {
    GetApp()->doDelSupplyForTeacher(this);
  }
  // 打印老师端所在的房间信息...
  if( m_lpRoom != NULL ) {
    m_lpRoom->doDumpRoomInfo();
  }
}

bool CTeacher::doServerSendDetect()
{
  // 只有老师推流端，服务器才会主动发起探测命令...
  if( this->GetIdTag() != ID_TAG_PUSHER )
    return false;
	// 采用了新的拥塞处理 => 删除指定缓存时间点之前的音视频数据包...
	this->doCalcAVJamStatus();
  // 填充探测命令包 => 服务器主动发起...
  m_server_rtp_detect.tm     = TM_TAG_SERVER;
  m_server_rtp_detect.id     = ID_TAG_SERVER;
  m_server_rtp_detect.pt     = PT_TAG_DETECT;
  m_server_rtp_detect.tsSrc  = (uint32_t)(os_gettime_ns() / 1000000);
  m_server_rtp_detect.dtDir  = DT_TO_SERVER;
  m_server_rtp_detect.dtNum += 1;
  // 获取需要的相关变量信息...
  uint32_t nHostAddr = this->GetHostAddr();
  uint16_t nHostPort = this->GetHostPort();
  int listen_fd = GetApp()->GetListenFD();
  if( listen_fd <= 0 || nHostAddr <= 0 || nHostPort <= 0 )
    return false;
  // 构造本对象自己的接收地址...
  sockaddr_in addrTeacher = {0};
  addrTeacher.sin_family = AF_INET;
  addrTeacher.sin_port = htons(nHostPort);
  addrTeacher.sin_addr.s_addr = htonl(nHostAddr);
  // 回复老师推流端 => 探测命令原样反馈信息...
  if( sendto(listen_fd, &m_server_rtp_detect, sizeof(m_server_rtp_detect), 0, (sockaddr*)&addrTeacher, sizeof(addrTeacher)) < 0 ) {
    log_trace("sendto error(code:%d, %s)", errno, strerror(errno));
    return false;
  }
  // 发送成功...
  return true;
}

void CTeacher::doCalcAVJamStatus()
{
	// 视频环形队列为空，没有拥塞，直接返回...
	if( m_video_circle.size <= 0 )
		return;
	// 遍历环形队列，删除所有超过n秒的缓存数据包 => 不管是否是关键帧或完整包，只是为补包而存在...
	const int nPerPackSize = DEF_MTU_SIZE + sizeof(rtp_hdr_t);
	static char szPacketBuffer[nPerPackSize] = {0};
	circlebuf & cur_circle = m_video_circle;
	rtp_hdr_t * lpCurHeader = NULL;
	uint32_t    min_ts = 0, min_seq = 0;
	uint32_t    max_ts = 0, max_seq = 0;
	// 读取第大的数据包的内容，获取最大时间戳...
	circlebuf_peek_back(&cur_circle, szPacketBuffer, nPerPackSize);
	lpCurHeader = (rtp_hdr_t*)szPacketBuffer;
	max_seq = lpCurHeader->seq;
	max_ts = lpCurHeader->ts;
	// 遍历环形队列，查看是否有需要删除的数据包...
	while ( cur_circle.size > 0 ) {
		// 读取第一个数据包的内容，获取最小时间戳...
		circlebuf_peek_front(&cur_circle, szPacketBuffer, nPerPackSize);
		lpCurHeader = (rtp_hdr_t*)szPacketBuffer;
		// 计算环形队列当中总的缓存时间...
		uint32_t cur_buf_ms = max_ts - lpCurHeader->ts;
		// 如果总缓存时间不超过n秒，中断操作...
		if (cur_buf_ms < 3000)
			break;
		assert(cur_buf_ms >= 3000);
		// 保存删除的时间点，供音频参考...
		min_ts = lpCurHeader->ts;
		min_seq = lpCurHeader->seq;
		// 如果总缓存时间超过n秒，删除最小数据包，继续寻找...
		circlebuf_pop_front(&cur_circle, NULL, nPerPackSize);
	}
	// 如果没有发生删除，直接返回...
	if (min_ts <= 0 || min_seq <= 0 )
		return;
	// 打印网络拥塞情况 => 就是视频缓存的拥塞情况...
	log_trace("[Teacher-Pusher] Video Jam => MinSeq: %u, MaxSeq: %u, Circle: %d", min_seq, max_seq, cur_circle.size/nPerPackSize);
	// 删除音频相关时间的数据包 => 包括这个时间戳之前的所有数据包都被删除...
	this->doEarseAudioByPTS(min_ts);
}
//
// 删除音频相关时间的数据包...
void CTeacher::doEarseAudioByPTS(uint32_t inTimeStamp)
{
	// 音频环形队列为空，直接返回...
	if (m_audio_circle.size <= 0)
		return;
	const int nPerPackSize = DEF_MTU_SIZE + sizeof(rtp_hdr_t);
	static char szPacketBuffer[nPerPackSize] = { 0 };
	circlebuf & cur_circle = m_audio_circle;
	rtp_hdr_t * lpCurHeader = NULL;
	uint32_t    min_seq = 0, max_seq = 0;
	// 读取第大的数据包的内容，获取最大序列号...
	circlebuf_peek_back(&cur_circle, szPacketBuffer, nPerPackSize);
	lpCurHeader = (rtp_hdr_t*)szPacketBuffer;
	max_seq = lpCurHeader->seq;
	// 遍历音频环形队列，删除所有时间戳小于输入时间戳的数据包...
	while( cur_circle.size > 0 ) {
		// 读取第一个数据包的内容，获取最小时间戳...
		circlebuf_peek_front(&cur_circle, szPacketBuffer, nPerPackSize);
		lpCurHeader = (rtp_hdr_t*)szPacketBuffer;
		// 如果当前包的时间戳大于输入时间戳，删除终止...
		if (lpCurHeader->ts > inTimeStamp)
			break;
		// 删除当前最小数据包，继续寻找...
		min_seq = lpCurHeader->seq;
		assert(lpCurHeader->ts <= inTimeStamp);
		circlebuf_pop_front(&cur_circle, NULL, nPerPackSize);
	}
	// 打印音频拥塞信息 => 当前位置，已发送数据包 => 两者之差就是观看端的有效补包空间...
	log_trace("[Teacher-Pusher] Audio Jam => MinSeq: %u, MaxSeq: %u, Circle: %d", min_seq, max_seq, cur_circle.size/nPerPackSize);
}
//
// 返回最小序号包，不用管是否是有效包号...
uint32_t CTeacher::doCalcMinSeq(bool bIsAudio)
{
	// 音视频使用不同队列和变量...
	circlebuf & cur_circle = bIsAudio ? m_audio_circle : m_video_circle;
  // 如果环形队列为空，直接返回0...
  if( cur_circle.size <= 0 )
    return 0;
  // 读取第一个数据包的内容，获取最小包序号...
	const int nPerPackSize = DEF_MTU_SIZE + sizeof(rtp_hdr_t);
	static char szPacketBuffer[nPerPackSize] = { 0 };
  circlebuf_peek_front(&cur_circle, szPacketBuffer, nPerPackSize);
  rtp_hdr_t * lpCurHeader = (rtp_hdr_t*)szPacketBuffer;
  return lpCurHeader->seq;
}

bool CTeacher::doTagDetect(char * lpBuffer, int inBufSize)
{
  // 老师观看者 => 把所有探测包都转发给学生推流者...
  // 注意：有两种探测包 => 老师观看者自己探测 + 老师观看者转发学生推流者探测...
  if( this->GetIdTag() == ID_TAG_LOOKER ) {
    return this->doTransferToStudentPusher(lpBuffer, inBufSize);
  }
  // 老师推流者 => 会收到两种探测反馈包 => 老师推流者自己 和 服务器...
  // 注意：需要通过分析探测包来判断发送者，做出不同的操作...
  assert( this->GetIdTag() == ID_TAG_PUSHER );
  // 通过第一个字节的低2位，判断终端类型...
  uint8_t tmTag = lpBuffer[0] & 0x03;
  // 获取第一个字节的中2位，得到终端身份...
  uint8_t idTag = (lpBuffer[0] >> 2) & 0x03;
  // 获取第一个字节的高4位，得到数据包类型...
  uint8_t ptTag = (lpBuffer[0] >> 4) & 0x0F;
  // 如果是 老师推流端 自己发出的探测包，原样反馈给老师推流者...
  if( tmTag == TM_TAG_TEACHER && idTag == ID_TAG_PUSHER ) {
    // 获取需要的相关变量信息...
    uint32_t nHostAddr = this->GetHostAddr();
    uint16_t nHostPort = this->GetHostPort();
    int listen_fd = GetApp()->GetListenFD();
    if( listen_fd <= 0 || nHostAddr <= 0 || nHostPort <= 0 )
      return false;
    // 构造本对象自己的接收地址...
    sockaddr_in addrTeacher = {0};
    addrTeacher.sin_family = AF_INET;
    addrTeacher.sin_port = htons(nHostPort);
    addrTeacher.sin_addr.s_addr = htonl(nHostAddr);
    // 回复老师推流端 => 探测命令原样反馈信息...
    if( sendto(listen_fd, lpBuffer, inBufSize, 0, (sockaddr*)&addrTeacher, sizeof(addrTeacher)) < 0 ) {
      log_trace("sendto error(code:%d, %s)", errno, strerror(errno));
      return false;
    }
    // 发送成功...
    return true;
  }
  // 如果是 服务器 发出的探测包，计算网络延时...
  if( tmTag == TM_TAG_SERVER && idTag == ID_TAG_SERVER ) {
		// 获取收到的探测数据包...
		rtp_detect_t rtpDetect = { 0 };
		memcpy(&rtpDetect, lpBuffer, sizeof(rtpDetect));
		// 当前时间转换成毫秒，计算网络延时 => 当前时间 - 探测时间...
		uint32_t cur_time_ms = (uint32_t)(os_gettime_ns() / 1000000);
		int keep_rtt = cur_time_ms - rtpDetect.tsSrc;
		// 防止网络突发延迟增大, 借鉴 TCP 的 RTT 遗忘衰减的算法...
		if (m_server_rtt_ms < 0) { m_server_rtt_ms = keep_rtt; }
		else { m_server_rtt_ms = (7 * m_server_rtt_ms + keep_rtt) / 8; }
		// 计算网络抖动的时间差值 => RTT的修正值...
		if (m_server_rtt_var_ms < 0) { m_server_rtt_var_ms = abs(m_server_rtt_ms - keep_rtt); }
		else { m_server_rtt_var_ms = (m_server_rtt_var_ms * 3 + abs(m_server_rtt_ms - keep_rtt)) / 4; }
		// 打印探测结果 => 探测序号 | 网络延时(毫秒)...
		log_debug("[Teacher-Pusher] Recv Detect => dtNum: %d, rtt: %d ms, rtt_var: %d ms", rtpDetect.dtNum, m_server_rtt_ms, m_server_rtt_var_ms);    
  }
  return true;
}

bool CTeacher::doTagCreate(char * lpBuffer, int inBufSize)
{
  //////////////////////////////////////////////////////
  // 注意：老师推流者和老师观看者处理方式会有不同...
  //////////////////////////////////////////////////////
  bool bResult = false;
  CApp * lpApp = GetApp();
  // 更新创建命令包内容，创建或更新房间，更新房间里的老师端...
  memcpy(&m_rtp_create, lpBuffer, sizeof(m_rtp_create));
  m_lpRoom = lpApp->doCreateRoom(m_rtp_create.roomID, m_rtp_create.liveID);
  m_lpRoom->doCreateTeacher(this);
  // 回复老师推流端 => 房间已经创建成功，不要再发创建命令了...
  if( this->GetIdTag() == ID_TAG_PUSHER ) {
    bResult = this->doCreateForPusher(lpBuffer, inBufSize);
  }
  // 回复老师观看端 => 将学生推流端的序列头转发给老师观看端 => 观看端收到后，会发送准备就绪命令...
  if( this->GetIdTag() == ID_TAG_LOOKER ) {
    bResult = this->doCreateForLooker(lpBuffer, inBufSize);
  }
  // 返回执行结果...
  return bResult;
}
//
// 回复老师推流端 => 房间已经创建成功，不要再发创建命令了...
bool CTeacher::doCreateForPusher(char * lpBuffer, int inBufSize)
{
  // 构造反馈的数据包...
  rtp_hdr_t rtpHdr = {0};
  rtpHdr.tm = TM_TAG_SERVER;
  rtpHdr.id = ID_TAG_SERVER;
  rtpHdr.pt = PT_TAG_CREATE;
  // 获取需要的相关变量信息...
  uint32_t nHostAddr = this->GetHostAddr();
  uint16_t nHostPort = this->GetHostPort();
  int listen_fd = GetApp()->GetListenFD();
  if( listen_fd <= 0 || nHostAddr <= 0 || nHostPort <= 0 )
    return false;
  // 构造本对象自己的接收地址...
	sockaddr_in addrTeacher = {0};
	addrTeacher.sin_family = AF_INET;
	addrTeacher.sin_port = htons(nHostPort);
	addrTeacher.sin_addr.s_addr = htonl(nHostAddr);
  // 回复老师推流端 => 房间已经创建成功，不要再发创建命令了...
  if( sendto(listen_fd, &rtpHdr, sizeof(rtpHdr), 0, (sockaddr*)&addrTeacher, sizeof(addrTeacher)) < 0 ) {
    log_trace("sendto error(code:%d, %s)", errno, strerror(errno));
    return false;
  }
  // 发送成功...
  return true;
}
//
// 回复老师观看端 => 将学生推流端的序列头转发给老师观看端 => 观看端收到后，会发送准备就绪命令...
bool CTeacher::doCreateForLooker(char * lpBuffer, int inBufSize)
{
  // 如果没有房间，直接返回...
  if( m_lpRoom == NULL )
    return false;
  // 获取房间里的学生推流者对象 => 无推流者，直接返回...
  CStudent * lpStudent = m_lpRoom->GetStudentPusher();
  if( lpStudent == NULL )
    return false;
  // 获取学生推流者的序列头信息 => 序列头为空，直接返回...
  string & strSeqHeader = lpStudent->GetSeqHeader();
  if( strSeqHeader.size() <= 0 )
    return false;
  // 获取需要的相关变量信息...
  uint32_t nHostAddr = this->GetHostAddr();
  uint16_t nHostPort = this->GetHostPort();
  int listen_fd = GetApp()->GetListenFD();
  if( listen_fd <= 0 || nHostAddr <= 0 || nHostPort <= 0 )
    return false;
  // 构造本对象自己的接收地址...
	sockaddr_in addrTeacher = {0};
	addrTeacher.sin_family = AF_INET;
	addrTeacher.sin_port = htons(nHostPort);
	addrTeacher.sin_addr.s_addr = htonl(nHostAddr);
  // 回复老师观看端 => 将学生推流端的序列头转发给老师观看端...
  if( sendto(listen_fd, strSeqHeader.c_str(), strSeqHeader.size(), 0, (sockaddr*)&addrTeacher, sizeof(addrTeacher)) < 0 ) {
    log_trace("sendto error(code:%d, %s)", errno, strerror(errno));
    return false;
  }
  // 发送成功...
  return true;
}

bool CTeacher::doTagDelete(char * lpBuffer, int inBufSize)
{
  // 注意：删除命令已经在CApp::doTagDelete()中拦截处理了...
  return true;
}

bool CTeacher::doTagSupply(char * lpBuffer, int inBufSize)
{
  // 只有老师观看者才会发起补包命令...
  if( this->GetIdTag() != ID_TAG_LOOKER )
    return false;
  // 将补包命令转发到学生推流端...
  return this->doTransferToStudentPusher(lpBuffer, inBufSize);
}

bool CTeacher::doTagHeader(char * lpBuffer, int inBufSize)
{
  // 只有老师推流者才会处理序列头命令...
  if( this->GetIdTag() != ID_TAG_PUSHER )
    return false;
  // 老师推流端是要服务多个学生端，直接保存序列头，有学生观看端接入时，再转发给对应的学生端...
  m_strSeqHeader.assign(lpBuffer, inBufSize);
  // 回复老师推流端 => 序列头已经收到，不要再发序列头命令了...
  return this->doHeaderForPusher(lpBuffer, inBufSize);
}
//
// 回复老师推流端 => 序列头已经收到，不要再发序列头命令了...
bool CTeacher::doHeaderForPusher(char * lpBuffer, int inBufSize)
{
  // 构造反馈的数据包...
  rtp_hdr_t rtpHdr = {0};
  rtpHdr.tm = TM_TAG_SERVER;
  rtpHdr.id = ID_TAG_SERVER;
  rtpHdr.pt = PT_TAG_HEADER;
  // 获取需要的相关变量信息...
  uint32_t nHostAddr = this->GetHostAddr();
  uint16_t nHostPort = this->GetHostPort();
  int listen_fd = GetApp()->GetListenFD();
  if( listen_fd <= 0 || nHostAddr <= 0 || nHostPort <= 0 )
    return false;
  // 构造本对象自己的接收地址...
	sockaddr_in addrTeacher = {0};
	addrTeacher.sin_family = AF_INET;
	addrTeacher.sin_port = htons(nHostPort);
	addrTeacher.sin_addr.s_addr = htonl(nHostAddr);
  // 回复老师推流端 => 序列头已经收到，不要再发序列头命令了...
  if( sendto(listen_fd, &rtpHdr, sizeof(rtpHdr), 0, (sockaddr*)&addrTeacher, sizeof(addrTeacher)) < 0 ) {
    log_trace("sendto error(code:%d, %s)", errno, strerror(errno));
    return false;
  }
  // 没有错误，直接返回...
  return true;
}

bool CTeacher::doTagReady(char * lpBuffer, int inBufSize)
{
  // 只有老师观看者才会处理准备就绪命令...
  if( this->GetIdTag() != ID_TAG_LOOKER )
    return false;
  // 老师观看者 => 转发学生推流者：已经收到序列头命令，观看端已经准备就绪...
  assert( this->GetIdTag() == ID_TAG_LOOKER );
  // 将自己的穿透地址更新到准备就绪结构体当中...
  rtp_ready_t * lpLookReady = (rtp_ready_t*)lpBuffer;
  lpLookReady->recvPort = this->GetHostPort();
  lpLookReady->recvAddr = this->GetHostAddr();
  // 转发学生推流者：已经收到序列头命令，观看端已经准备就绪...
  return this->doTransferToStudentPusher(lpBuffer, inBufSize);
}

bool CTeacher::doTagAudio(char * lpBuffer, int inBufSize)
{
  // 只有老师推流端才会处理音频包命令...
  if( this->GetIdTag() != ID_TAG_PUSHER )
    return false;
  // 将音频数据包缓存起来...
  this->doTagAVPackProcess(lpBuffer, inBufSize);
  // 转发音频数据包到所有的学生观看者...
  return this->doTransferToStudentLooker(lpBuffer, inBufSize);
}

bool CTeacher::doTagVideo(char * lpBuffer, int inBufSize)
{
  // 只有老师推流端才会处理视频包命令...
  if( this->GetIdTag() != ID_TAG_PUSHER )
    return false;
  // 将视频数据包缓存起来...
  this->doTagAVPackProcess(lpBuffer, inBufSize);
  // 转发视频数据包到所有的学生观看者...
  return this->doTransferToStudentLooker(lpBuffer, inBufSize);
}

void CTeacher::doTagAVPackProcess(char * lpBuffer, int inBufSize)
{
	// 判断输入数据包的有效性 => 不能小于数据包的头结构长度...
	const int nPerPackSize = DEF_MTU_SIZE + sizeof(rtp_hdr_t);
	if( lpBuffer == NULL || inBufSize < sizeof(rtp_hdr_t) || inBufSize > nPerPackSize ) {
		log_trace("[Teacher-Pusher] Error => RecvLen: %d, Max: %d", inBufSize, nPerPackSize);
		return;
	}
	// 如果收到的缓冲区长度不够 或 填充量为负数，直接丢弃...
	rtp_hdr_t * lpNewHeader = (rtp_hdr_t*)lpBuffer;
	int nDataSize = lpNewHeader->psize + sizeof(rtp_hdr_t);
	int nZeroSize = DEF_MTU_SIZE - lpNewHeader->psize;
	uint8_t  pt_tag = lpNewHeader->pt;
	uint32_t new_id = lpNewHeader->seq;
	uint32_t max_id = new_id;
	uint32_t min_id = new_id;
	// 出现打包错误，丢掉错误包，打印错误信息...
	if( inBufSize != nDataSize || nZeroSize < 0 ) {
		log_trace("[Teacher-Pusher] Error => RecvLen: %d, DataSize: %d, ZeroSize: %d", inBufSize, nDataSize, nZeroSize);
		return;
	}
	// 音视频使用不同的打包对象和变量...
	circlebuf & cur_circle = (pt_tag == PT_TAG_AUDIO) ? m_audio_circle : m_video_circle;
	// 首先，将当前包序列号从丢包队列当中删除...
	this->doEraseLoseSeq(pt_tag, new_id);
	//////////////////////////////////////////////////////////////////////////////////////////////////
	// 注意：每个环形队列中的数据包大小是一样的 => rtp_hdr_t + slice + Zero => 12 + 800 => 812
	//////////////////////////////////////////////////////////////////////////////////////////////////
	static char szPacketBuffer[nPerPackSize] = {0};
	// 如果环形队列为空 => 需要对丢包做提前预判并进行处理...
	if( cur_circle.size < nPerPackSize ) {
		// 新到序号包与最大播放包之间有空隙，说明有丢包...
		// 丢包闭区间 => [0 + 1, new_id - 1]
		if( new_id > (0 + 1) ) {
			this->doFillLosePack(pt_tag, 0 + 1, new_id - 1);
		}
		// 把最新序号包直接追加到环形队列的最后面，如果与最大播放包之间有空隙，已经在前面的步骤中补充好了...
		// 先加入包头和数据内容...
		circlebuf_push_back(&cur_circle, lpBuffer, inBufSize);
		// 再加入填充数据内容，保证数据总是保持一个MTU单元大小...
		if( nZeroSize > 0 ) {
			circlebuf_push_back_zero(&cur_circle, nZeroSize);
		}
		// 打印新追加的序号包 => 不管有没有丢包，都要追加这个新序号包...
		//log_trace("[Teacher-Pusher] Max Seq: %u, Cricle: Zero", new_id);
		return;
	}
	// 环形队列中至少要有一个数据包...
	assert( cur_circle.size >= nPerPackSize );
	// 获取环形队列中最小序列号...
	circlebuf_peek_front(&cur_circle, szPacketBuffer, nPerPackSize);
	rtp_hdr_t * lpMinHeader = (rtp_hdr_t*)szPacketBuffer;
	min_id = lpMinHeader->seq;
	// 获取环形队列中最大序列号...
	circlebuf_peek_back(&cur_circle, szPacketBuffer, nPerPackSize);
	rtp_hdr_t * lpMaxHeader = (rtp_hdr_t*)szPacketBuffer;
	max_id = lpMaxHeader->seq;
	// 发生丢包条件 => max_id + 1 < new_id
	// 丢包闭区间 => [max_id + 1, new_id - 1];
	if( max_id + 1 < new_id ) {
		this->doFillLosePack(pt_tag, max_id + 1, new_id - 1);
	}
	// 如果是丢包或正常序号包，放入环形队列，返回...
	if( max_id + 1 <= new_id ) {
		// 先加入包头和数据内容...
		circlebuf_push_back(&cur_circle, lpBuffer, inBufSize);
		// 再加入填充数据内容，保证数据总是保持一个MTU单元大小...
		if( nZeroSize > 0 ) {
			circlebuf_push_back_zero(&cur_circle, nZeroSize);
		}
		// 打印新加入的最大序号包...
		//log_trace("[Teacher-Pusher] Max Seq: %u, Circle: %d", new_id, cur_circle.size/nPerPackSize-1);
		return;
	}
	// 如果是丢包后的补充包 => max_id > new_id
	if( max_id > new_id ) {
		// 如果最小序号大于丢包序号 => 打印错误，直接丢掉这个补充包...
		if( min_id > new_id ) {
			log_trace("[Teacher-Pusher] Supply Discard => Seq: %u, Min-Max: [%u, %u], Type: %d", new_id, min_id, max_id, pt_tag);
			return;
		}
		// 最小序号不能比丢包序号小...
		assert( min_id <= new_id );
		// 计算缓冲区更新位置...
		uint32_t nPosition = (new_id - min_id) * nPerPackSize;
		// 将获取的数据内容更新到指定位置...
		circlebuf_place(&cur_circle, nPosition, lpBuffer, inBufSize);
		// 打印补充包信息...
		log_trace("[Teacher-Pusher] Supply Success => Seq: %u, Min-Max: [%u, %u], Type: %d", new_id, min_id, max_id, pt_tag);
		return;
	}
	// 如果是其它未知包，打印信息...
	log_trace("[Teacher-Pusher] Supply Unknown => Seq: %u, Slice: %d, Min-Max: [%u, %u], Type: %d", new_id, lpNewHeader->psize, min_id, max_id, pt_tag);
}
//
// 查看当前包是否需要从丢包队列中删除...
void CTeacher::doEraseLoseSeq(uint8_t inPType, uint32_t inSeqID)
{
	// 根据数据包类型，找到丢包集合...
	GM_MapLose & theMapLose = (inPType == PT_TAG_AUDIO) ? m_AudioMapLose : m_VideoMapLose;
	// 如果没有找到指定的序列号，直接返回...
	GM_MapLose::iterator itorItem = theMapLose.find(inSeqID);
	if( itorItem == theMapLose.end() )
		return;
	// 删除检测到的丢包节点...
	rtp_lose_t & rtpLose = itorItem->second;
	uint32_t nResendCount = rtpLose.resend_count;
	theMapLose.erase(itorItem);
	// 打印已收到的补包信息，还剩下的未补包个数...
	log_trace("[Teacher-Pusher] Supply Erase => LoseSeq: %u, ResendCount: %u, LoseSize: %u, Type: %d", inSeqID, nResendCount, theMapLose.size(), inPType);
}
//
// 给丢失数据包预留环形队列缓存空间...
void CTeacher::doFillLosePack(uint8_t inPType, uint32_t nStartLoseID, uint32_t nEndLoseID)
{
	// 根据数据包类型，找到丢包集合...
	circlebuf & cur_circle = (inPType == PT_TAG_AUDIO) ? m_audio_circle : m_video_circle;
	GM_MapLose & theMapLose = (inPType == PT_TAG_AUDIO) ? m_AudioMapLose : m_VideoMapLose;
	// 需要对网络抖动时间差进行线路选择 => 只有一条服务器线路...
	int cur_rtt_var_ms = m_server_rtt_var_ms;
	// 准备数据包结构体并进行初始化 => 连续丢包，设置成相同的重发时间点，否则，会连续发非常多的补包命令...
	uint32_t cur_time_ms = (uint32_t)(os_gettime_ns() / 1000000);
	uint32_t sup_id = nStartLoseID;
	rtp_hdr_t rtpDis = {0};
	rtpDis.pt = PT_TAG_LOSE;
	// 注意：是闭区间 => [nStartLoseID, nEndLoseID]
	while( sup_id <= nEndLoseID ) {
		// 给当前丢包预留缓冲区...
		rtpDis.seq = sup_id;
		circlebuf_push_back(&cur_circle, &rtpDis, sizeof(rtpDis));
		circlebuf_push_back_zero(&cur_circle, DEF_MTU_SIZE);
		// 将丢包序号加入丢包队列当中 => 毫秒时刻点...
		rtp_lose_t rtpLose = {0};
		rtpLose.resend_count = 0;
		rtpLose.lose_seq = sup_id;
		rtpLose.lose_type = inPType;
		// 注意：这里要避免 网络抖动时间差 为负数的情况 => 还没有完成第一次探测的情况，也不能为0，会猛烈发包...
		// 重发时间点 => cur_time + rtt_var => 丢包时的当前时间 + 丢包时的网络抖动时间差 => 避免不是丢包，只是乱序的问题...
		rtpLose.resend_time = cur_time_ms + max(cur_rtt_var_ms, MAX_SLEEP_MS);
		theMapLose[sup_id] = rtpLose;
		// 打印已丢包信息，丢包队列长度...
		log_trace("[Teacher-Pusher] Lose Seq: %u, LoseSize: %u, Type: %d", sup_id, theMapLose.size(), inPType);
		// 累加当前丢包序列号...
		++sup_id;
	}
  // 把自己加入到补包对象列表当中...
  GetApp()->doAddSupplyForTeacher(this);
}

int CTeacher::doServerSendSupply()
{
  // -1 => 音视频都没有补包...
  //  0 => 有补包，但不到补包时间...
  //  1 => 有补包，已经发送补包命令...
  int nRetAudio = this->doSendSupplyCmd(true);
  int nRetVideo = this->doSendSupplyCmd(false);
  // 如果音视频都小于0，返回-1...
  if( nRetAudio < 0 && nRetVideo < 0 )
    return -1;
  // 只要有一个大于0，返回1...
  if( nRetAudio > 0 || nRetVideo > 0 )
    return 1;
  // 其余的情况返回0...
  return 0;
}

int CTeacher::doSendSupplyCmd(bool bIsAudio)
{
	// 根据数据包类型，找到丢包集合...
	circlebuf & cur_circle = bIsAudio ? m_audio_circle : m_video_circle;
	GM_MapLose & theMapLose = bIsAudio ? m_AudioMapLose : m_VideoMapLose;
	// 如果丢包集合队列为空，直接返回...
	if( theMapLose.size() <= 0 )
		return -1;
	assert( theMapLose.size() > 0 );
	// 定义最大的补包缓冲区...
	const int nHeadSize = sizeof(rtp_supply_t);
	const int nPerPackSize = DEF_MTU_SIZE + nHeadSize;
	static char szPacketBuffer[nPerPackSize] = {0};
	uint32_t min_id = 0;
	// 获取环形队列中最小序列号...
  if( cur_circle.size > nPerPackSize ) {
    circlebuf_peek_front(&cur_circle, szPacketBuffer, nPerPackSize);
    rtp_hdr_t * lpMinHeader = (rtp_hdr_t*)szPacketBuffer;
    min_id = lpMinHeader->seq;
  }
  // 获取补包缓冲区存放数据的头指针...
	char * lpData = szPacketBuffer + nHeadSize;
	// 获取当前时间的毫秒值 => 小于或等于当前时间的丢包都需要通知发送端再次发送...
	uint32_t cur_time_ms = (uint32_t)(os_gettime_ns() / 1000000);
	// 需要对网络往返延迟值进行线路选择 => 只有一条服务器线路...
	int cur_rtt_ms = m_server_rtt_ms;
	// 重置补报长度为0 => 重新计算需要补包的个数...
  // 需要设置为从服务器发出的补包命令...
  rtp_supply_t rtpSupply = {0};
  rtpSupply.tm = TM_TAG_SERVER;
  rtpSupply.id = ID_TAG_SERVER;
  rtpSupply.pt = PT_TAG_SUPPLY;
  rtpSupply.suSize = 0;
	rtpSupply.suType = bIsAudio ? PT_TAG_AUDIO : PT_TAG_VIDEO;
	// 遍历丢包队列，找出需要补包的丢包序列号...
	GM_MapLose::iterator itorItem = theMapLose.begin();
	while( itorItem != theMapLose.end() ) {
		rtp_lose_t & rtpLose = itorItem->second;
    // 如果要补的包号，比最小包号还要小，直接丢弃，已经过期了...
    if( rtpLose.lose_seq < min_id ) {
      log_trace("[Teacher-Pusher] Supply Discard => LoseSeq: %u, MinSeq: %u, Audio: %d", rtpLose.lose_seq, min_id, bIsAudio);
      theMapLose.erase(itorItem++);
      continue;
    }
    // 补包序号在有效范围之内...
		if( rtpLose.resend_time <= cur_time_ms ) {
			// 如果补包缓冲超过设定的最大值，跳出循环 => 最多补包200个...
			if( (nHeadSize + rtpSupply.suSize) >= nPerPackSize )
				break;
			// 累加补包长度和指针，拷贝补包序列号...
			memcpy(lpData, &rtpLose.lose_seq, sizeof(uint32_t));
			rtpSupply.suSize += sizeof(uint32_t);
			lpData += sizeof(uint32_t);
			// 累加重发次数...
			++rtpLose.resend_count;
			// 注意：同时发送的补包，下次也同时发送，避免形成多个散列的补包命令...
			// 注意：如果一个网络往返延时都没有收到补充包，需要再次发起这个包的补包命令...
			// 注意：这里要避免 网络抖动时间差 为负数的情况 => 还没有完成第一次探测的情况，也不能为0，会猛烈发包...
			// 修正下次重传时间点 => cur_time + rtt => 丢包时的当前时间 + 网络往返延迟值 => 需要进行线路选择...
			rtpLose.resend_time = cur_time_ms + max(cur_rtt_ms, MAX_SLEEP_MS);
			// 如果补包次数大于1，下次补包不要太快，追加一个休息周期..
			rtpLose.resend_time += ((rtpLose.resend_count > 1) ? MAX_SLEEP_MS : 0);
		}
		// 累加丢包算子对象...
		++itorItem;
	}
	// 如果补充包缓冲为空 => 补包时间未到...
	if( rtpSupply.suSize <= 0 )
		return 0;
	// 更新补包命令头内容块...
	memcpy(szPacketBuffer, &rtpSupply, nHeadSize);
	// 如果补包缓冲不为空，才进行补包命令发送...
	int nDataSize = nHeadSize + rtpSupply.suSize;
  // 获取需要的相关变量信息...
  int listen_fd = GetApp()->GetListenFD();
  uint32_t nHostAddr = this->GetHostAddr();
  uint16_t nHostPort = this->GetHostPort();
  if( listen_fd <= 0 || nHostAddr <= 0 || nHostPort <= 0 )
    return 0;
  // 构造观看者的接收地址...
	sockaddr_in addrTeacher = {0};
	addrTeacher.sin_family = AF_INET;
	addrTeacher.sin_port = htons(nHostPort);
	addrTeacher.sin_addr.s_addr = htonl(nHostAddr);
  // 将序列头通过房间对象，转发给老师观看者...
  if( sendto(listen_fd, szPacketBuffer, nDataSize, 0, (sockaddr*)&addrTeacher, sizeof(addrTeacher)) < 0 ) {
    log_trace("sendto error(code:%d, %s)", errno, strerror(errno));
    return 0;
  }
	// 打印已发送补包命令...
	log_trace("[Teacher-Pusher] Supply Send => Dir: %d, Count: %d, Audio: %d", DT_TO_SERVER, rtpSupply.suSize/sizeof(uint32_t), bIsAudio);
  // 成功发送补包命令，返回1...
  return 1;
}

bool CTeacher::doTransferToStudentPusher(char * lpBuffer, int inBufSize)
{
  // 如果没有房间，直接返回...
  if( m_lpRoom == NULL )
    return false;
  // 获取房间里的学生推流者对象 => 无推流者，直接返回...
  CStudent * lpStudent = m_lpRoom->GetStudentPusher();
  if( lpStudent == NULL )
    return false;
  // 获取需要的相关变量信息...
  uint32_t nHostAddr = lpStudent->GetHostAddr();
  uint16_t nHostPort = lpStudent->GetHostPort();
  int listen_fd = GetApp()->GetListenFD();
  if( listen_fd <= 0 || nHostAddr <= 0 || nHostPort <= 0 )
    return false;
  // 构造观看者的接收地址...
	sockaddr_in addrStudent = {0};
	addrStudent.sin_family = AF_INET;
	addrStudent.sin_port = htons(nHostPort);
	addrStudent.sin_addr.s_addr = htonl(nHostAddr);
  // 将序列头通过房间对象，转发给老师观看者...
  if( sendto(listen_fd, lpBuffer, inBufSize, 0, (sockaddr*)&addrStudent, sizeof(addrStudent)) < 0 ) {
    log_trace("sendto error(code:%d, %s)", errno, strerror(errno));
    return false;
  }
  // 没有错误，直接返回...
  return true;
}

bool CTeacher::doTransferToStudentLooker(char * lpBuffer, int inBufSize)
{
  // 如果没有房间，直接返回...
  if( m_lpRoom == NULL )
    return false;
  // 转发命令数据包到所有房间里的学生在线观看者...
  return m_lpRoom->doTransferToStudentLooker(lpBuffer, inBufSize);
}