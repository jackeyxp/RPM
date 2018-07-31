
#include "app.h"
#include "network.h"

CNetwork::CNetwork(uint8_t tmTag, uint8_t idTag, uint32_t inHostAddr, uint16_t inHostPort)
  : m_nHostAddr(inHostAddr)
  , m_nHostPort(inHostPort)
  , m_tmTag(tmTag)
  , m_idTag(idTag)
  , m_lpRoom(NULL)
{
  m_nStartTime = time(NULL);
  memset(&m_rtp_create, 0, sizeof(rtp_create_t));
}

CNetwork::~CNetwork()
{
  // 通过相互关联的TCP连接通知终端UDP连接退出了...
  GetApp()->doLogoutForUDP(m_rtp_create.tcpSock, this->GetTmTag(), this->GetIdTag());
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
