
#include "tcproom.h"
#include "tcpclient.h"
#include "tcpcamera.h"

CTCPRoom::CTCPRoom(int inRoomID)
  : m_lpTCPTeacher(NULL)
  , m_nRoomID(inRoomID)
{
  
}

CTCPRoom::~CTCPRoom()
{
  this->clearAllCamera();
}

// 删除房间里所有有效的摄像头对象...
void CTCPRoom::clearAllCamera()
{
  GM_MapTCPCamera::iterator itorItem;
  for(itorItem = m_MapTCPCamera.begin(); itorItem != m_MapTCPCamera.end(); ++itorItem) {
    delete itorItem->second;
  }
  m_MapTCPCamera.clear();
}

// 创建摄像头 => 由房间来管理摄像头的生命周期...
CTCPCamera * CTCPRoom::doCreateCamera(int nDBCameraID, int nTCPSockFD, string & strCameraName, string & strPCName)
{
  // 如果找到了摄像头对象，更新数据...
  CTCPCamera * lpTCPCamera = NULL;
  GM_MapTCPCamera::iterator itorItem = m_MapTCPCamera.find(nDBCameraID);
  if( itorItem != m_MapTCPCamera.end() ) {
    lpTCPCamera = itorItem->second;
    lpTCPCamera->SetRoomID(m_nRoomID);
    lpTCPCamera->SetDBCameraID(nDBCameraID);
    lpTCPCamera->SetTCPSockFD(nTCPSockFD);
    lpTCPCamera->SetPCName(strPCName);
    lpTCPCamera->SetCameraName(strCameraName);
    return lpTCPCamera;
  }
  // 如果没有找到摄像头对象，创建一个新的对象...
  lpTCPCamera = new CTCPCamera(m_nRoomID, nDBCameraID, nTCPSockFD, strCameraName, strPCName);
  m_MapTCPCamera[nDBCameraID] = lpTCPCamera;
  return lpTCPCamera;
}

// 删除摄像头 => 学生端发起的停止命令...
void CTCPRoom::doDeleteCamera(int nDBCameraID)
{
  // 如果找到了摄像头对象，直接删除之...
  GM_MapTCPCamera::iterator itorItem = m_MapTCPCamera.find(nDBCameraID);
  if (itorItem != m_MapTCPCamera.end()) {
    delete itorItem->second;
    m_MapTCPCamera.erase(itorItem);
  }
}

// 一个房间可以有多个学生观看端...
void CTCPRoom::doCreateStudent(CTCPClient * lpStudent)
{
  int nConnFD = lpStudent->GetConnFD();
  int nClientType = lpStudent->GetClientType();
  // 如果终端类型不是学生观看端，直接返回...
  if( nClientType != kClientStudent )
    return;
  // 将学生观看到更新到观看列表...
  m_MapTCPStudent[nConnFD] = lpStudent;
}

// 注意：这里还需要删除与这个学生端相关的在线摄像头对象...
void CTCPRoom::doDeleteStudent(CTCPClient * lpStudent)
{
  // 先获取套接字号码，用来查找学生端和摄像头...
  int nConnFD = lpStudent->GetConnFD();
  // 遍历房间里的摄像头列表，通过套接字进行匹配...
  CTCPCamera * lpTCPCamera = NULL;
  GM_MapTCPCamera::iterator itorCamera = m_MapTCPCamera.begin();
  while(itorCamera != m_MapTCPCamera.end()) {
    lpTCPCamera = itorCamera->second;
    // 如果摄像头的套接字与当前学生端的套接字不一致，继续下一个...
    if( lpTCPCamera->GetTCPSockFD() != nConnFD) {
      ++itorCamera; continue;
    }
    // 套接字是一致的，删除摄像头，列表中删除...
    delete lpTCPCamera; lpTCPCamera = NULL;
    m_MapTCPCamera.erase(itorCamera++);
  }
  // 找到相关观看学生端对象，直接删除返回...
  GM_MapTCPConn::iterator itorItem = m_MapTCPStudent.find(nConnFD);
  if( itorItem != m_MapTCPStudent.end() ) {
    m_MapTCPStudent.erase(itorItem);
    return;
  }
  // 如果通过FD方式没有找到，通过指针遍历查找...
  itorItem = m_MapTCPStudent.begin();
  while(itorItem != m_MapTCPStudent.end()) {
    // 找到了相关节点 => 删除节点，返回...
    if(itorItem->second == lpStudent) {
      m_MapTCPStudent.erase(itorItem);
      return;
    }
  }
  // 通过指针遍历也没有找到，打印错误信息...
  log_trace("Can't find TCP-Student, ConnFD: %d", nConnFD);
}

// 一个房间只有一个老师端...
void CTCPRoom::doCreateTeacher(CTCPClient * lpTeacher)
{
  int nClientType = lpTeacher->GetClientType();
  // 如果终端类型不是讲师端，直接返回...
  if( nClientType != kClientTeacher )
    return;
  // 保存讲师端连接对象...
  m_lpTCPTeacher = lpTeacher;
}

void CTCPRoom::doDeleteTeacher(CTCPClient * lpTeacher)
{
  // 如果是房间里的老师端，置空，返回...
  if( m_lpTCPTeacher == lpTeacher ) {
    m_lpTCPTeacher = NULL;
    return;
  }
}

// 告诉房间里在线的TCP讲师端 => 房间里的学生推流端上线或下线了...
void CTCPRoom::doUDPStudentPusherOnLine(int inDBCameraID, bool bIsOnLineFlag)
{
  // 房间里没有老师存在，直接返回...
  if( m_lpTCPTeacher == NULL )
    return;
  m_lpTCPTeacher->doUDPStudentPusherOnLine(inDBCameraID, bIsOnLineFlag);
}

// 告诉所有在线的TCP学生端 => 房间里的讲师推流端上线了...
void CTCPRoom::doUDPTeacherPusherOnLine(bool bIsOnLineFlag)
{
  if( m_MapTCPStudent.size() <= 0 )
    return;
  GM_MapTCPConn::iterator itorItem;
  for(itorItem = m_MapTCPStudent.begin(); itorItem != m_MapTCPStudent.end(); ++itorItem) {
    CTCPClient * lpStudent = itorItem->second;
    if( lpStudent == NULL ) continue;
    lpStudent->doUDPTeacherPusherOnLine(bIsOnLineFlag);
  }  
}
