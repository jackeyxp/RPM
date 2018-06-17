
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
  CApp * lpApp = GetApp();
  memcpy(&m_rtp_create, lpBuffer, sizeof(m_rtp_create));
  m_lpRoom = lpApp->doCreateRoom(m_rtp_create.roomID, m_rtp_create.liveID);
  m_lpRoom->doCreateTeacher(this);
  return true;
}

bool CTeacher::doTagDelete(char * lpBuffer, int inBufSize)
{
  return true;
}

bool CTeacher::doTagSupply(char * lpBuffer, int inBufSize)
{
  return true;
}

bool CTeacher::doTagHeader(char * lpBuffer, int inBufSize)
{
  // 只有老师推流者才会处理序列头命令...
  if( this->GetIdTag() != ID_TAG_PUSHER )
    return false;
  // 老师推流端是要服务多个学生端，直接保存序列头，有学生观看端接入时，再转发给对应的学生端...
  m_strSeqHeader.assign(lpBuffer, inBufSize);
  return true;
}

bool CTeacher::doTagReady(char * lpBuffer, int inBufSize)
{
  // 只有老师观看端才会处理准备就绪命令...
  if( this->GetIdTag() != ID_TAG_LOOKER )
    return false;
  // 将穿透地址更新到准备就绪结构体当中...
  rtp_ready_t * lpReady = (rtp_ready_t*)lpBuffer;
  lpReady->recvPort = this->GetHostPort();
  lpReady->recvAddr = this->GetHostAddr();
  // 将准备就绪命令转发到学生推流端...
  return this->doTransferToStudentPusher(lpBuffer, inBufSize);
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