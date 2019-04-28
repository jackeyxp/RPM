
#pragma once

#include "network.h"
#include "circlebuf.h"

class CUDPThread;
class CTeacher : public CNetwork
{
public:
  CTeacher(CUDPThread * lpUDPThread, uint8_t tmTag, uint8_t idTag, uint32_t inHostAddr, uint16_t inHostPort);
  virtual ~CTeacher();
public:
  circlebuf  &  GetAudioCircle() { return m_audio_circle; }
  circlebuf  &  GetVideoCircle() { return m_video_circle; }
public:
  uint32_t      doCalcMinSeq(bool bIsAudio);
  bool          doIsTeacherPusherLose(bool bIsAudio, uint32_t inLoseSeq);
protected:
  virtual bool  doTagDetect(char * lpBuffer, int inBufSize);
  virtual bool  doTagCreate(char * lpBuffer, int inBufSize);
  virtual bool  doTagDelete(char * lpBuffer, int inBufSize);
  virtual bool  doTagSupply(char * lpBuffer, int inBufSize);
  virtual bool  doTagHeader(char * lpBuffer, int inBufSize);
  virtual bool  doTagReady(char * lpBuffer, int inBufSize);
  virtual bool  doTagAudio(char * lpBuffer, int inBufSize);
  virtual bool  doTagVideo(char * lpBuffer, int inBufSize);
  virtual bool  doServerSendDetect();
  virtual int   doServerSendSupply();
  virtual bool  doServerSendLose();
private:
  int           doSendSupplyCmd(bool bIsAudio);
  void          doSendLosePacket(bool bIsAudio);
  uint32_t      doCalcMaxConSeq(bool bIsAudio);

  bool          doIsStudentPusherLose(bool bIsAudio, uint32_t inLoseSeq);
  bool          doCreateForPusher(char * lpBuffer, int inBufSize);
  bool          doCreateForLooker(char * lpBuffer, int inBufSize);
  bool          doHeaderForPusher(char * lpBuffer, int inBufSize);

  bool          doTransferToStudentLooker(char * lpBuffer, int inBufSize);
  bool          doDetectForLooker(char * lpBuffer, int inBufSize);

  void          doCalcAVJamStatus();
  void          doEarseAudioByPTS(uint32_t inTimeStamp);

  void          doTagAVPackProcess(char * lpBuffer, int inBufSize);
  void          doEraseLoseSeq(uint8_t inPType, uint32_t inSeqID);
  void          doFillLosePack(uint8_t inPType, uint32_t nStartLoseID, uint32_t nEndLoseID);
private:
  int           m_server_rtt_ms;      // Server => ���������ӳ�ֵ => ���� => ����ʱ������̽����ʱ
  int           m_server_rtt_var_ms;  // Server => ���綶��ʱ��� => ���� => ����ʱ������̽����ʱ
  rtp_detect_t  m_server_rtp_detect;  // Server => ̽������...
  circlebuf     m_audio_circle;       // ��������Ƶ���ζ���...
  circlebuf     m_video_circle;       // ��������Ƶ���ζ���...
  GM_MapLose    m_AudioMapLose;			  // �����˼��|�ۿ����ϱ�����Ƶ�������϶���...
  GM_MapLose    m_VideoMapLose;			  // �����˼��|�ۿ����ϱ�����Ƶ�������϶���...
  CUDPThread  * m_lpUDPThread; 
};