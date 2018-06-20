
#include "app.h"
#include "room.h"
#include "student.h"
#include "teacher.h"

CTeacher::CTeacher(uint8_t tmTag, uint8_t idTag, uint32_t inHostAddr, uint16_t inHostPort)
  : CNetwork(tmTag, idTag, inHostAddr, inHostPort)
{
  m_strSeqHeader.clear();
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
}

bool CTeacher::doTagDetect(char * lpBuffer, int inBufSize)
{
  bool bResult = false;
  // 老师观看者 => 把所有探测包都转发给学生推流者...
  // 注意：有两种探测包 => 老师观看者自己探测 + 老师观看者转发学生推流者探测...
  if( this->GetIdTag() == ID_TAG_LOOKER ) {
    bResult = this->doTransferToStudentPusher(lpBuffer, inBufSize);
  }
  // 老师推流者 => 把所有探测包都转发给学生观看者...
  // 注意：有两种探测包 => 老师推流者自己探测 + 老师推流者转发学生观看者探测...
  if( this->GetIdTag() == ID_TAG_PUSHER ) {
    bResult = this->doTransferToStudentLooker(lpBuffer, inBufSize);
  }
  // 返回执行结果...
  return bResult;
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
  // 没有错误，直接返回...
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
  // 打印服务器反馈给老师观看端被创建的信息...
  //log_debug("[Teacher] doCreateForLooker => %lu:%lu", nHostAddr, nHostPort);
  // 没有错误，直接返回...
  return true;
}

bool CTeacher::doTagDelete(char * lpBuffer, int inBufSize)
{
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
  bool bResult = false;
  // 老师观看者 => 转发学生推流者：已经收到序列头命令，观看端已经准备就绪...
  if( this->GetIdTag() == ID_TAG_LOOKER ) {
    // 将自己的穿透地址更新到准备就绪结构体当中...
    rtp_ready_t * lpLookReady = (rtp_ready_t*)lpBuffer;
    lpLookReady->recvPort = this->GetHostPort();
    lpLookReady->recvAddr = this->GetHostAddr();
    // 转发学生推流者：已经收到序列头命令，观看端已经准备就绪...
    bResult = this->doTransferToStudentPusher(lpBuffer, inBufSize);
  }
  // 注意：这里需要特殊处理 => 学生观看者可能是多个，需要老师推流者指定回复哪个学生观看者...
  // 注意：指定对象就在收到的 rtp_ready_t 结构体当中...
  // 老师推流者 => 回复学生观看者：已经收到准备就绪命令，不要再发送准备就绪命令了...
  //if( this->GetIdTag() == ID_TAG_PUSHER ) {
  //  bResult = this->doTransferToStudentLooker(lpBuffer, inBufSize);
  //}
  // 返回执行结果...
  return bResult;
}

bool CTeacher::doTagAudio(char * lpBuffer, int inBufSize)
{
  // 只有老师推流端才会处理音频包命令...
  if( this->GetIdTag() != ID_TAG_PUSHER )
    return false;
  // 转发音频包命令到所有的学生观看者...
  return this->doTransferToStudentLooker(lpBuffer, inBufSize);
}

bool CTeacher::doTagVideo(char * lpBuffer, int inBufSize)
{
  // 只有老师推流端才会处理视频包命令...
  if( this->GetIdTag() != ID_TAG_PUSHER )
    return false;
  // 转发视频包命令到所有的学生观看者...
  return this->doTransferToStudentLooker(lpBuffer, inBufSize);
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
  // 转发命令数据包到所有房间里的观看者...
  return m_lpRoom->doTransferToStudentLooker(lpBuffer, inBufSize);
}