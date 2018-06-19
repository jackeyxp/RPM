
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
}

bool CStudent::doTagDetect(char * lpBuffer, int inBufSize)
{
  bool bResult = false;
  // 学生推流者 => 把所有探测包都转发给老师观看者...
  // 注意：有两种探测包 => 学生推流者自己探测 + 学生推流者转发老师观看者探测...
  if( this->GetIdTag() == ID_TAG_PUSHER ) {
    bResult = this->doTransferToTeacherLooker(lpBuffer, inBufSize);
  }
  // 学生观看者 => 把所有探测包都转发给老师推流者...
  // 注意：有两种探测包 => 学生观看者自己探测 + 学生观看者转发老师推流者探测...
  if( this->GetIdTag() == ID_TAG_LOOKER ) {
    bResult = this->doTransferToTeacherPusher(lpBuffer, inBufSize);
  }
  // 返回执行结果...
  return bResult;
}

bool CStudent::doTagCreate(char * lpBuffer, int inBufSize)
{
  //////////////////////////////////////////////////////
  // 注意：学生推流者和学生观看者处理方式会有不同...
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
  // 只有学生观看者才会发起补包命令
  if( this->GetIdTag() != ID_TAG_LOOKER )
    return false;
  // 将补包命令转发到老师推流端...
  return this->doTransferToTeacherPusher(lpBuffer, inBufSize);
}

bool CStudent::doTagHeader(char * lpBuffer, int inBufSize)
{
  // 只有学生推流端才会处理序列头命令...
  if( this->GetIdTag() != ID_TAG_PUSHER )
    return false;
  // 转发序列头命令到老师观看端对象...
  return this->doTransferToTeacherLooker(lpBuffer, inBufSize);
}

bool CStudent::doTagReady(char * lpBuffer, int inBufSize)
{
  // 只有学生观看者才会处理准备就绪命令...
  if( this->GetIdTag() != ID_TAG_LOOKER )
    return false;
  // 转发准备就绪命令到老师推流者对象...
  return this->doTransferToTeacherPusher(lpBuffer, inBufSize);
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
  // 没有错误，直接返回...
  return true;
}

bool CStudent::doTransferToTeacherPusher(char * lpBuffer, int inBufSize)
{
  // 如果没有房间，直接返回...
  if( m_lpRoom == NULL )
    return false;
  // 获取房间里的老师推流者对象 => 无推流者，直接返回...
  CTeacher * lpTeacher = m_lpRoom->GetTeacherPusher();
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
  // 没有错误，直接返回...
  return true;
}

bool CStudent::doTransferByRoom(char * lpBuffer, int inBufSize)
{
  if( lpBuffer == NULL || inBufSize <= 0 )
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
  // 将序列头通过房间对象，转发给老师观看者...
  if( sendto(listen_fd, lpBuffer, inBufSize, 0, (sockaddr*)&addrStudent, sizeof(addrStudent)) < 0 ) {
    log_trace("sendto error(code:%d, %s)", errno, strerror(errno));
    return false;
  }
  // 没有错误，直接返回...
  return true; 
}