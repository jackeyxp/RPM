
#include "tcproom.h"
#include "tcpclient.h"

CTCPRoom::CTCPRoom(int inRoomID)
  : m_lpTCPTeacher(NULL)
  , m_nRoomID(inRoomID)
{
  
}

CTCPRoom::~CTCPRoom()
{
  
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

void CTCPRoom::doDeleteStudent(CTCPClient * lpStudent)
{
  // 在学生观看端中遍历查找...
  int nConnFD = lpStudent->GetConnFD();
  GM_MapTCPConn::iterator itorItem = m_MapTCPStudent.find(nConnFD);
  // 找到相关观看学生端对象，直接删除返回...
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