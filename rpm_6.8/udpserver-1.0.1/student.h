
#pragma once

#include "network.h"

class CRoom;
class CStudent : public CNetwork
{
public:
  CStudent(uint8_t tmTag, uint8_t idTag, uint32_t inHostAddr, uint16_t inHostPort);
  virtual ~CStudent();
public:
  bool          doTransferByRoom(char * lpBuffer, int inBufSize);
protected:
  virtual bool  doTagDetect(char * lpBuffer, int inBufSize);
  virtual bool  doTagCreate(char * lpBuffer, int inBufSize);
  virtual bool  doTagDelete(char * lpBuffer, int inBufSize);
  virtual bool  doTagSupply(char * lpBuffer, int inBufSize);
  virtual bool  doTagHeader(char * lpBuffer, int inBufSize);
  virtual bool  doTagReady(char * lpBuffer, int inBufSize);
  virtual bool  doTagAudio(char * lpBuffer, int inBufSize);
  virtual bool  doTagVideo(char * lpBuffer, int inBufSize);
private:
  bool          doCreateForPusher(char * lpBuffer, int inBufSize);
  bool          doCreateForLooker(char * lpBuffer, int inBufSize);
  bool          doHeaderForPusher(char * lpBuffer, int inBufSize);

  bool          doTransferToTeacherLooker(char * lpBuffer, int inBufSize);
  bool          doTransferToTeacherPusher(char * lpBuffer, int inBufSize);
};