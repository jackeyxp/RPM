
#pragma once

#include "global.h"

class CTCPCamera
{
public:
  CTCPCamera(int nRoomID, int nDBCameraID, int nTCPSockFD, string & strCameraName, string & strPCName);
  ~CTCPCamera();
public:
  void        SetRoomID(int nRoomID) { m_nRoomID = nRoomID; }
  void        SetDBCameraID(int nDBCameraID) { m_nDBCameraID = nDBCameraID; }
  void        SetTCPSockFD(int nTCPSockFD) { m_nTCPSockFD = nTCPSockFD; }
  void        SetPCName(string & strPCName) { m_strPCName = strPCName; }
  void        SetCameraName(string & strCameraName) { m_strCameraName = strCameraName; }
  int         GetRoomID() { return m_nRoomID; }
  int         GetDBCameraID() { return m_nDBCameraID; }
  int         GetTCPSockFD() { return m_nTCPSockFD; }
  string   &  GetPCName() { return m_strPCName; }
  string   &  GetCameraName() { return m_strCameraName; }
private:
  int         m_nRoomID;
  int         m_nTCPSockFD;
  int         m_nDBCameraID;
  string      m_strPCName;
  string      m_strCameraName;
};
