
#include "tcpcamera.h"

CTCPCamera::CTCPCamera(int nRoomID, int nDBCameraID, int nTCPSockFD, string & strCameraName, string & strPCName)
  : m_strCameraName(strCameraName)
  , m_nDBCameraID(nDBCameraID)
  , m_nTCPSockFD(nTCPSockFD)
  , m_strPCName(strPCName)
  , m_nRoomID(nRoomID)
{
  
}

CTCPCamera::~CTCPCamera()
{
  
}
