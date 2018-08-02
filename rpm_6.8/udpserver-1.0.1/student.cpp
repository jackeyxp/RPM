
#include "app.h"
#include "room.h"
#include "student.h"
#include "teacher.h"

CStudent::CStudent(uint8_t tmTag, uint8_t idTag, uint32_t inHostAddr, uint16_t inHostPort)
  : CNetwork(tmTag, idTag, inHostAddr, inHostPort)
{
  // 打印学生端被创建信息...
  log_debug("[Student-Create] tmTag: %d, idTag: %d", tmTag, idTag);
}

CStudent::~CStudent()
{
  // 打印学生端被删除信息...
  log_debug("[Student-Delete] tmTag: %d, idTag: %d", this->GetTmTag(), this->GetIdTag());
  // 在房间中注销本学生端对象...
  if( m_lpRoom != NULL ) {
    m_lpRoom->doDeleteStudent(this);
  }
  // 如果是观看端，把自己从丢包队列当中删除掉...
  if( this->GetIdTag() == ID_TAG_LOOKER ) {
    GetApp()->doDelLoseForStudent(this);
  }
  // 打印学生端所在的房间信息...
  if( m_lpRoom != NULL ) {
    m_lpRoom->doDumpRoomInfo();
  }
}

bool CStudent::doServerSendDetect()
{
  // 注意：学生端都不进行服务器主动探测...
  return false;  
}

bool CStudent::doTagDetect(char * lpBuffer, int inBufSize)
{
  bool bResult = false;
  // 学生推流者 => 把所有探测包都转发给老师观看者...
  // 注意：有两种探测包 => 学生推流者自己探测 + 学生推流者转发老师观看者探测...
  if( this->GetIdTag() == ID_TAG_PUSHER ) {
    bResult = this->doTransferToTeacherLooker(lpBuffer, inBufSize);
  }
  // 学生观看者 => 只有一种探测包 => 学生观看者自己的探测包...
  // 学生观看者 => 将探测包原样返回给自己，计算网络往返延时...
  if( this->GetIdTag() == ID_TAG_LOOKER ) {
    bResult = this->doDetectForLooker(lpBuffer, inBufSize);
  }
  // 返回执行结果...
  return bResult;
}

bool CStudent::doDetectForLooker(char * lpBuffer, int inBufSize)
{
  // 如果没有房间，直接返回...
  if( m_lpRoom == NULL )
    return false;
  // 获取房间里的老师推流者对象 => 无推流者，直接返回...
  CTeacher * lpTeacher = m_lpRoom->GetTeacherPusher();
  if( lpTeacher == NULL )
    return false;
  // 注意：音视频最小序号包不用管是否是有效包，只简单获取最小包号...
  // 将老师推流者当前最小的音视频数据包号更新到探测包当中...
  rtp_detect_t * lpDetect = (rtp_detect_t*)lpBuffer;
  lpDetect->maxAConSeq = lpTeacher->doCalcMinSeq(true);
  lpDetect->maxVConSeq = lpTeacher->doCalcMinSeq(false);
  return this->doTransferToStudentLooker(lpBuffer, inBufSize);
}

bool CStudent::doTagCreate(char * lpBuffer, int inBufSize)
{
  //////////////////////////////////////////////////////
  // 注意：学生推流者和学生观看者处理方式会有不同...
  //////////////////////////////////////////////////////
  bool bResult = false;
  CApp * lpApp = GetApp();
  // 更新创建命令包内容，创建或更新房间，更新房间里的学生端...
  memcpy(&m_rtp_create, lpBuffer, sizeof(m_rtp_create));
  m_lpRoom = lpApp->doCreateRoom(m_rtp_create.roomID);
  m_lpRoom->doCreateStudent(this);
  // 回复学生推流端 => 房间已经创建成功，不要再发创建命令了...
  if( this->GetIdTag() == ID_TAG_PUSHER ) {
    bResult = this->doCreateForPusher(lpBuffer, inBufSize);
  }
  // 回复学生观看端 => 将老师推流端的序列头转发给学生观看端...
  if( this->GetIdTag() == ID_TAG_LOOKER ) {
    bResult = this->doCreateForLooker(lpBuffer, inBufSize);
  }
  // 返回执行结果...
  return bResult;
}
//
// 回复学生推流端 => 房间已经创建成功，不要再发创建命令了...
bool CStudent::doCreateForPusher(char * lpBuffer, int inBufSize)
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
	sockaddr_in addrStudent = {0};
	addrStudent.sin_family = AF_INET;
	addrStudent.sin_port = htons(nHostPort);
	addrStudent.sin_addr.s_addr = htonl(nHostAddr);
  // 回复学生推流端 => 房间已经创建成功，不要再发创建命令了...
  if( sendto(listen_fd, &rtpHdr, sizeof(rtpHdr), 0, (sockaddr*)&addrStudent, sizeof(addrStudent)) < 0 ) {
    log_trace("sendto error(code:%d, %s)", errno, strerror(errno));
    return false;
  }
  // 发送成功...
  return true;
}
//
// 回复学生观看端 => 将老师推流端的序列头转发给学生观看端...
bool CStudent::doCreateForLooker(char * lpBuffer, int inBufSize)
{
  // 如果没有房间，直接返回...
  if( m_lpRoom == NULL )
    return false;
  // 获取房间里的老师推流者对象 => 无推流者，直接返回...
  CTeacher * lpTeacher = m_lpRoom->GetTeacherPusher();
  if( lpTeacher == NULL )
    return false;
  // 获取老师推流者的序列头信息 => 序列头为空，直接返回...
  string & strSeqHeader = lpTeacher->GetSeqHeader();
  if( strSeqHeader.size() <= 0 )
    return false;
  // 获取需要的相关变量信息...
  uint32_t nHostAddr = this->GetHostAddr();
  uint16_t nHostPort = this->GetHostPort();
  int listen_fd = GetApp()->GetListenFD();
  if( listen_fd <= 0 || nHostAddr <= 0 || nHostPort <= 0 )
    return false;
  // 构造本对象自己的接收地址...
	sockaddr_in addrStudent = {0};
	addrStudent.sin_family = AF_INET;
	addrStudent.sin_port = htons(nHostPort);
	addrStudent.sin_addr.s_addr = htonl(nHostAddr);
  // 回复学生观看端 => 将老师推流端的序列头转发给学生观看端...
  if( sendto(listen_fd, strSeqHeader.c_str(), strSeqHeader.size(), 0, (sockaddr*)&addrStudent, sizeof(addrStudent)) < 0 ) {
    log_trace("sendto error(code:%d, %s)", errno, strerror(errno));
    return false;
  }
  // 发送成功...
  return true;
}

bool CStudent::doTagDelete(char * lpBuffer, int inBufSize)
{
  // 注意：删除命令已经在CApp::doTagDelete()中拦截处理了...
  return true;
}

bool CStudent::doTagSupply(char * lpBuffer, int inBufSize)
{
  // 判断输入数据的内容是否有效，如果无效，直接返回...
  if( lpBuffer == NULL || inBufSize <= 0 || inBufSize < sizeof(rtp_supply_t) )
    return false;
  // 只有学生观看者才会发起补包命令
  if( this->GetIdTag() != ID_TAG_LOOKER )
    return false;
  // 通过第一个字节的低2位，判断终端类型...
  uint8_t tmTag = lpBuffer[0] & 0x03;
  // 获取第一个字节的中2位，得到终端身份...
  uint8_t idTag = (lpBuffer[0] >> 2) & 0x03;
  // 获取第一个字节的高4位，得到数据包类型...
  uint8_t ptTag = (lpBuffer[0] >> 4) & 0x0F;
  // 注意：只处理 学生观看端 发出的补包命令...
  if( tmTag != TM_TAG_STUDENT || idTag != ID_TAG_LOOKER )
    return false;
  // 注意：学生观看端的补包从服务器上的老师推流者的缓存获取...
  rtp_supply_t rtpSupply = {0};
  int nHeadSize = sizeof(rtp_supply_t);
  memcpy(&rtpSupply, lpBuffer, nHeadSize);
  if( (rtpSupply.suSize <= 0) || ((nHeadSize + rtpSupply.suSize) != inBufSize) )
    return false;
  // 根据数据包类型，找到丢包集合...
  bool bIsAudio = (rtpSupply.suType == PT_TAG_AUDIO) ? true : false;
  GM_MapLose & theMapLose = bIsAudio ? m_AudioMapLose : m_VideoMapLose;
  // 获取需要补包的序列号，加入到补包队列当中...
  char * lpDataPtr = lpBuffer + nHeadSize;
  int    nDataSize = rtpSupply.suSize;
  while( nDataSize > 0 ) {
    uint32_t   nLoseSeq = 0;
    rtp_lose_t rtpLose = {0};
    // 获取补包序列号...
    memcpy(&nLoseSeq, lpDataPtr, sizeof(int));
    // 移动数据区指针位置...
    lpDataPtr += sizeof(int);
    nDataSize -= sizeof(int);
    // 查看这个丢包号是否是服务器端也要补的包...
    // 服务器收到补包后会自动转发，这里就不用补了...
    if( this->doIsServerLose(bIsAudio, nLoseSeq) )
      continue;
    // 是观看端丢失的新包，需要进行补包队列处理...
    // 如果序列号已经存在，增加补包次数，不存在，增加新记录...
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
  // 如果补包队列为空 => 都是服务器端本身就需要补的包...
  if( theMapLose.size() <= 0 )
    return true;
  // 把自己加入到丢包对象列表当中...
  GetApp()->doAddLoseForStudent(this);
  // 打印已收到补包命令...
  log_trace("[Student-Looker] Supply Recv => Count: %d, Type: %d", rtpSupply.suSize / sizeof(int), rtpSupply.suType);
  return true;
}

bool CStudent::doIsServerLose(bool bIsAudio, uint32_t inLoseSeq)
{
  // 如果没有房间，直接返回...
  if( m_lpRoom == NULL )
    return false;
  // 获取房间里的老师推流者对象 => 无推流者，直接返回...
  CTeacher * lpTeacher = m_lpRoom->GetTeacherPusher();
  if( lpTeacher == NULL )
    return false;
  // 在老师推流端中查看是否在对应的补包队列当中...
  return lpTeacher->doIsServerLose(bIsAudio, inLoseSeq);
}

bool CStudent::doServerSendLose()
{
  // 发送观看端需要的音频丢失数据包...
  this->doSendLosePacket(true);
  // 发送观看端需要的视频丢失数据包...
  this->doSendLosePacket(false);
  // 如果音频和视频都没有丢包数据，返回false...
  if( m_AudioMapLose.size() <= 0 && m_VideoMapLose.size() <= 0 ) {
    return false;
  }
  // 音视频只要有一个还有补包序号，返回true...
  return true;
}

void CStudent::doSendLosePacket(bool bIsAudio)
{
  // 根据数据包类型，找到丢包集合、环形队列...
  GM_MapLose & theMapLose = bIsAudio ? m_AudioMapLose : m_VideoMapLose;
  // 丢包集合队列为空，直接返回...
  if( theMapLose.size() <= 0 )
    return;
  // 拿出一个丢包记录，无论是否发送成功，都要删除这个丢包记录...
  // 如果观看端，没有收到这个数据包，会再次发起补包命令...
  GM_MapLose::iterator itorItem = theMapLose.begin();
  rtp_lose_t rtpLose = itorItem->second;
  theMapLose.erase(itorItem);
  // 如果没有房间，直接返回...
  if( m_lpRoom == NULL )
    return;
  // 获取房间里的老师推流者对象 => 无推流者，直接返回...
  CTeacher * lpTeacher = m_lpRoom->GetTeacherPusher();
  if( lpTeacher == NULL )
    return;
  // 获取老师推流者在服务器缓存的音频或视频环形队列对象...
  circlebuf & cur_circle = bIsAudio ? lpTeacher->GetAudioCircle() : lpTeacher->GetVideoCircle();
  // 如果环形队列为空，直接返回...
  if( cur_circle.size <= 0 )
    return;
  // 先找到环形队列中最前面数据包的头指针 => 最小序号...
  rtp_hdr_t * lpFrontHeader = NULL;
  rtp_hdr_t * lpSendHeader = NULL;
  int nSendPos = 0, nSendSize = 0;
  /////////////////////////////////////////////////////////////////////////////////////////////////
  // 注意：千万不能在环形队列当中进行指针操作，当start_pos > end_pos时，可能会有越界情况...
  // 所以，一定要用接口读取完整的数据包之后，再进行操作；如果用指针，一旦发生回还，就会错误...
  /////////////////////////////////////////////////////////////////////////////////////////////////
  const int nPerPackSize = DEF_MTU_SIZE + sizeof(rtp_hdr_t);
  static char szPacketBuffer[nPerPackSize] = {0};
  circlebuf_peek_front(&cur_circle, szPacketBuffer, nPerPackSize);
  lpFrontHeader = (rtp_hdr_t*)szPacketBuffer;
  // 如果要补充的数据包序号比最小序号还要小 => 没有找到，直接返回...
  if( rtpLose.lose_seq < lpFrontHeader->seq ) {
    log_trace("[Student-Looker] Supply Error => lose: %u, min: %u, Type: %d", rtpLose.lose_seq, lpFrontHeader->seq, rtpLose.lose_type);
    return;
  }
  assert( rtpLose.lose_seq >= lpFrontHeader->seq );
  // 注意：环形队列当中的序列号一定是连续的...
  // 两者之差就是要发送数据包的头指针位置...
  nSendPos = (rtpLose.lose_seq - lpFrontHeader->seq) * nPerPackSize;
  // 如果补包位置大于或等于环形队列长度 => 补包越界...
  if( nSendPos >= cur_circle.size ) {
    log_trace("[Student-Looker] Supply Error => Position Excessed");
    return;
  }
  ////////////////////////////////////////////////////////////////////////////////////////////////////
  // 注意：不能用简单的指针操作，环形队列可能会回还，必须用接口 => 从指定相对位置拷贝指定长度数据...
  // 获取将要发送数据包的包头位置和有效数据长度...
  ////////////////////////////////////////////////////////////////////////////////////////////////////
  memset(szPacketBuffer, 0, nPerPackSize);
  circlebuf_read(&cur_circle, nSendPos, szPacketBuffer, nPerPackSize);
  lpSendHeader = (rtp_hdr_t*)szPacketBuffer;
  // 如果找到的序号位置不对 或 本身就是需要补的丢包...
  if((lpSendHeader->pt == PT_TAG_LOSE) || (lpSendHeader->seq != rtpLose.lose_seq)) {
    log_trace("[Student-Looker] Supply Error => Seq: %u, Find: %u, Type: %d", rtpLose.lose_seq, lpSendHeader->seq, lpSendHeader->pt);
    return;
  }
  // 获取有效的数据区长度 => 包头 + 数据...
  nSendSize = sizeof(rtp_hdr_t) + lpSendHeader->psize;
  // 获取需要的相关变量信息...
  uint32_t nHostAddr = this->GetHostAddr();
  uint16_t nHostPort = this->GetHostPort();
  int listen_fd = GetApp()->GetListenFD();
  if( listen_fd <= 0 || nHostAddr <= 0 || nHostPort <= 0 )
    return;
  // 构造本对象自己的接收地址...
  sockaddr_in addrStudent = {0};
  addrStudent.sin_family = AF_INET;
  addrStudent.sin_port = htons(nHostPort);
  addrStudent.sin_addr.s_addr = htonl(nHostAddr);
  // 回复学生推流端 => 序列头已经收到，不要再发序列头命令了...
  if( sendto(listen_fd, (void*)lpSendHeader, nSendSize, 0, (sockaddr*)&addrStudent, sizeof(addrStudent)) < 0 ) {
    log_trace("sendto error(code:%d, %s)", errno, strerror(errno));
    return;
  }
  // 打印已经发送补包信息...
  log_trace("[Student-Looker Supply Send => Seq: %u, TS: %u, Slice: %d, Type: %d", lpSendHeader->seq, lpSendHeader->ts, lpSendHeader->psize, lpSendHeader->pt);
}

bool CStudent::doTagHeader(char * lpBuffer, int inBufSize)
{
  // 只有学生推流端才会处理序列头命令...
  if( this->GetIdTag() != ID_TAG_PUSHER )
    return false;
  // 将序列头保存起来，等待观看端接入时，转发给观看端...
  m_strSeqHeader.assign(lpBuffer, inBufSize);
  // 回复学生推流端 => 序列头已经收到，不要再发序列头命令了...
  return this->doHeaderForPusher(lpBuffer, inBufSize);
}
//
// 回复学生推流端 => 序列头已经收到，不要再发序列头命令了...
bool CStudent::doHeaderForPusher(char * lpBuffer, int inBufSize)
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
	sockaddr_in addrStudent = {0};
	addrStudent.sin_family = AF_INET;
	addrStudent.sin_port = htons(nHostPort);
	addrStudent.sin_addr.s_addr = htonl(nHostAddr);
  // 回复学生推流端 => 序列头已经收到，不要再发序列头命令了...
  if( sendto(listen_fd, &rtpHdr, sizeof(rtpHdr), 0, (sockaddr*)&addrStudent, sizeof(addrStudent)) < 0 ) {
    log_trace("sendto error(code:%d, %s)", errno, strerror(errno));
    return false;
  }
  // 发送成功...
  return true;
}

bool CStudent::doTagReady(char * lpBuffer, int inBufSize)
{
  // 只有学生推流者才会处理准备就绪命令...
  if( this->GetIdTag() != ID_TAG_PUSHER )
    return false;
  // 学生推流者 => 回复老师观看者：已经收到准备就绪命令，不要再发送准备就绪命令了...
  assert( this->GetIdTag() == ID_TAG_PUSHER );
  // 将自己的穿透地址更新到准备就绪结构体当中...
  rtp_ready_t * lpPushReady = (rtp_ready_t*)lpBuffer;
  lpPushReady->recvPort = this->GetHostPort();
  lpPushReady->recvAddr = this->GetHostAddr();
  // 回复老师观看者：已经收到准备就绪命令，不要再发送准备就绪命令了...
  return this->doTransferToTeacherLooker(lpBuffer, inBufSize);
}

bool CStudent::doTagAudio(char * lpBuffer, int inBufSize)
{
  // 只有学生推流端才会处理音频包命令...
  if( this->GetIdTag() != ID_TAG_PUSHER )
    return false;
  // 转发音频包命令到老师观看端对象...
  return this->doTransferToTeacherLooker(lpBuffer, inBufSize);
}

bool CStudent::doTagVideo(char * lpBuffer, int inBufSize)
{
  // 只有学生推流端才会处理视频包命令...
  if( this->GetIdTag() != ID_TAG_PUSHER )
    return false;
  // 转发视频包命令到老师观看端对象...
  return this->doTransferToTeacherLooker(lpBuffer, inBufSize);
}

bool CStudent::doTransferToTeacherLooker(char * lpBuffer, int inBufSize)
{
  // 如果没有房间，直接返回...
  if( m_lpRoom == NULL )
    return false;
  // 获取房间里的老师观看者对象 => 无观看者，直接返回...
  CTeacher * lpTeacher = m_lpRoom->GetTeacherLooker();
  if( lpTeacher == NULL )
    return false;
  // 获取需要的相关变量信息...
  uint32_t nHostAddr = lpTeacher->GetHostAddr();
  uint16_t nHostPort = lpTeacher->GetHostPort();
  int listen_fd = GetApp()->GetListenFD();
  if( listen_fd <= 0 || nHostAddr <= 0 || nHostPort <= 0 )
    return false;
  // 构造观看者的接收地址...
	sockaddr_in addrTeacher = {0};
	addrTeacher.sin_family = AF_INET;
	addrTeacher.sin_port = htons(nHostPort);
	addrTeacher.sin_addr.s_addr = htonl(nHostAddr);
  // 将序列头通过房间对象，转发给老师观看者...
  if( sendto(listen_fd, lpBuffer, inBufSize, 0, (sockaddr*)&addrTeacher, sizeof(addrTeacher)) < 0 ) {
    log_trace("sendto error(code:%d, %s)", errno, strerror(errno));
    return false;
  }
  // 发送成功...
  return true;
}

bool CStudent::doTransferToStudentLooker(char * lpBuffer, int inBufSize)
{
  // 判断输入的缓冲区是否有效...
  if( lpBuffer == NULL || inBufSize <= 0 )
    return false;
  // 这里只处理针对学生观看者的数据转发...
  if( this->GetIdTag() != ID_TAG_LOOKER )
    return false;
   // 获取需要的相关变量信息...
  uint32_t nHostAddr = this->GetHostAddr();
  uint16_t nHostPort = this->GetHostPort();
  int listen_fd = GetApp()->GetListenFD();
  if( listen_fd <= 0 || nHostAddr <= 0 || nHostPort <= 0 )
    return false;
  // 构造观看者的接收地址...
	sockaddr_in addrStudent = {0};
	addrStudent.sin_family = AF_INET;
	addrStudent.sin_port = htons(nHostPort);
	addrStudent.sin_addr.s_addr = htonl(nHostAddr);
  // 将数据信息转发给学生观看者对象...
  if( sendto(listen_fd, lpBuffer, inBufSize, 0, (sockaddr*)&addrStudent, sizeof(addrStudent)) < 0 ) {
    log_trace("sendto error(code:%d, %s)", errno, strerror(errno));
    return false;
  }
  // 发送成功...
  return true; 
}