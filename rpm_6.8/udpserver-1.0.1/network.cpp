
#include "app.h"
#include "network.h"

CNetwork::CNetwork(uint8_t tmTag, uint8_t idTag, uint32_t inHostAddr, uint16_t inHostPort)
  : m_nHostAddr(inHostAddr)
  , m_nHostPort(inHostPort)
  , m_bIsDeleteByTCP(false)
  , m_bIsDeleteByUDP(false)
  , m_tmTag(tmTag)
  , m_idTag(idTag)
  , m_lpRoom(NULL)
{
  m_nStartTime = time(NULL);
  memset(&m_rtp_create, 0, sizeof(rtp_create_t));
}

CNetwork::~CNetwork()
{
  // 如果是被TCP删除的，不要通知TCP关联终端...
  if( m_bIsDeleteByTCP ) {
    log_trace("[UDP-%s-%s-Delete] by TCP, HostPort: %d, LiveID: %d",
               get_tm_tag(m_tmTag), get_id_tag(m_idTag),
               this->GetHostPort(), this->GetDBCameraID());
    return;
  }
  // 不是被TCP删除的，就是被UDP删除的...
  m_bIsDeleteByUDP = true;
  log_trace("[UDP-%s-%s-Delete] by UDP, HostPort: %d, LiveID: %d",
             get_tm_tag(m_tmTag), get_id_tag(m_idTag),
             this->GetHostPort(), this->GetDBCameraID());
  // 通过相互关联的TCP连接通知终端UDP连接退出了...
  GetApp()->doUDPLogoutToTCP(m_rtp_create.tcpSock, m_rtp_create.liveID, this->GetTmTag(), this->GetIdTag());
}

void CNetwork::ResetTimeout()
{
  m_nStartTime = time(NULL);
}

bool CNetwork::IsTimeout()
{
  time_t nDeltaTime = time(NULL) - m_nStartTime;
  return ((nDeltaTime >= PLAY_TIME_OUT) ? true : false);  
}

bool CNetwork::doProcess(uint8_t ptTag, char * lpBuffer, int inBufSize)
{
  // 收到网络数据包，重置超时计数器...
  this->ResetTimeout();

  // 对网络数据包，进行分发操作...  
  bool bResult = false;
  switch( ptTag )
  {
    case PT_TAG_DETECT:  bResult = this->doTagDetect(lpBuffer, inBufSize); break;
    case PT_TAG_CREATE:  bResult = this->doTagCreate(lpBuffer, inBufSize); break;
    case PT_TAG_DELETE:  bResult = this->doTagDelete(lpBuffer, inBufSize); break;
    case PT_TAG_SUPPLY:  bResult = this->doTagSupply(lpBuffer, inBufSize); break;
    case PT_TAG_HEADER:  bResult = this->doTagHeader(lpBuffer, inBufSize); break;
    case PT_TAG_READY:   bResult = this->doTagReady(lpBuffer, inBufSize);  break;
    case PT_TAG_AUDIO:   bResult = this->doTagAudio(lpBuffer, inBufSize);  break;
    case PT_TAG_VIDEO:   bResult = this->doTagVideo(lpBuffer, inBufSize);  break;
  }
  return bResult;
}

// 原路返回的转发接口 => 观看者|推流者都可以原路返回...
bool CNetwork::doTransferToFrom(char * lpBuffer, int inBufSize)
{
  // 判断输入的缓冲区是否有效...
  if( lpBuffer == NULL || inBufSize <= 0 )
    return false;
   // 获取需要的相关变量信息...
  uint32_t nHostAddr = this->GetHostAddr();
  uint16_t nHostPort = this->GetHostPort();
  int listen_fd = GetApp()->GetListenFD();
  if( listen_fd <= 0 || nHostAddr <= 0 || nHostPort <= 0 )
    return false;
  // 构造返回的接收地址...
	sockaddr_in addrFrom = {0};
	addrFrom.sin_family = AF_INET;
	addrFrom.sin_port = htons(nHostPort);
	addrFrom.sin_addr.s_addr = htonl(nHostAddr);
  // 将数据信息转发给学生观看者对象...
  if( sendto(listen_fd, lpBuffer, inBufSize, 0, (sockaddr*)&addrFrom, sizeof(addrFrom)) < 0 ) {
    log_trace("sendto error(code:%d, %s)", errno, strerror(errno));
    return false;
  }
  //log_debug("[Transfer] Size: %d", inBufSize);
  // 发送成功...
  return true; 
}
