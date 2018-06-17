
#pragma once

#include "global.h"

class CNetwork
{
public:
  CNetwork(uint8_t tmTag, uint8_t idTag, uint32_t inHostAddr, uint16_t inHostPort);
  virtual ~CNetwork();
public:
  bool          IsTimeout();
  void          ResetTimeout();
  uint8_t       GetTmTag() { return m_tmTag; }
  uint8_t       GetIdTag() { return m_idTag; }
  uint32_t      GetHostAddr() { return m_nHostAddr; }
  uint16_t      GetHostPort() { return m_nHostPort; }
  bool          doProcess(uint8_t ptTag, char * lpBuffer, int inBufSize);
protected:
  virtual bool  doTagDetect(char * lpBuffer, int inBufSize) = 0;
  virtual bool  doTagCreate(char * lpBuffer, int inBufSize) = 0;
  virtual bool  doTagDelete(char * lpBuffer, int inBufSize) = 0;
  virtual bool  doTagSupply(char * lpBuffer, int inBufSize) = 0;
  virtual bool  doTagHeader(char * lpBuffer, int inBufSize) = 0;
  virtual bool  doTagReady(char * lpBuffer, int inBufSize) = 0;
  virtual bool  doTagAudio(char * lpBuffer, int inBufSize) = 0;
  virtual bool  doTagVideo(char * lpBuffer, int inBufSize) = 0;
protected:
  uint32_t      m_nHostAddr;
  uint16_t      m_nHostPort;
  uint8_t       m_tmTag;
  uint8_t       m_idTag;
  CRoom    *    m_lpRoom;
  rtp_create_t  m_rtp_create;
  time_t        m_nStartTime;       // 超时检测起点
};