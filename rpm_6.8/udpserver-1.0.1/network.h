
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
  string    &   GetSeqHeader() { return m_strSeqHeader; }
  int           GetDBCameraID() { return m_rtp_create.liveID; }
  int           GetTCPSockID() { return m_rtp_create.tcpSock; }
  void          SetDeleteByTCP() { m_bIsDeleteByTCP = true; }
  bool          GetDeleteByTCP() { return m_bIsDeleteByTCP; }
  bool          GetDeleteByUDP() { return m_bIsDeleteByUDP; }
  bool          doProcess(uint8_t ptTag, char * lpBuffer, int inBufSize);
  bool          doTransferToFrom(char * lpBuffer, int inBufSize);
public:
  virtual bool  doServerSendDetect() = 0;
  virtual int   doServerSendSupply() = 0;
  virtual bool  doServerSendLose() = 0;
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
  uint32_t      m_nHostAddr;        // 映射地址
  uint16_t      m_nHostPort;        // 映射端口
  uint8_t       m_tmTag;            // 终端类型
  uint8_t       m_idTag;            // 终端标识
  CRoom    *    m_lpRoom;           // 房间对象
  rtp_create_t  m_rtp_create;       // 创建命令
  time_t        m_nStartTime;       // 超时检测起点
  string        m_strSeqHeader;     // 推流端上传的序列头命令包...
  bool          m_bIsDeleteByTCP;   // 是否被TCP终端连接删除标志...
  bool          m_bIsDeleteByUDP;   // 是否被UDP终端连接删除标志...
};