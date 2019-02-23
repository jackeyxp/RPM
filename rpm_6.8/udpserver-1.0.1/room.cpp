
#include "app.h"
#include "room.h"
#include "student.h"
#include "teacher.h"

CRoom::CRoom(int inRoomID)
  : m_lpStudentPusher(NULL)
  , m_lpTeacherPusher(NULL)
  , m_lpTeacherLooker(NULL)
  , m_wExAudioChangeNum(65530)
  , m_nRoomID(inRoomID)
{
  
}

CRoom::~CRoom()
{
  
}

void CRoom::doDumpRoomInfo()
{
  log_trace("\n======== RoomID: %d ========\n Student-Pusher: %d, Teacher-Looker: %d\n Teacher-Pusher: %d, Student-Looker: %d", m_nRoomID, 
            ((m_lpStudentPusher != NULL) ? 1 : 0), ((m_lpTeacherLooker != NULL) ? 1 : 0), ((m_lpTeacherPusher != NULL) ? 1 : 0), m_MapStudentLooker.size());
}
//
// 根据传递过来的学生端类型，进行变量更新...
void CRoom::doCreateStudent(CStudent * lpStudent)
{
  int nHostPort = lpStudent->GetHostPort();
  uint8_t idTag = lpStudent->GetIdTag();
  if( idTag == ID_TAG_PUSHER ) {
    // 如果新的学生推流对象与原有对象不相同(可能多次发命令) => 告诉房间里的TCP讲师端，可以创建拉流线程了...
    if( m_lpStudentPusher != lpStudent ) {
      m_lpStudentPusher = lpStudent;
      GetApp()->doUDPStudentPusherOnLine(m_nRoomID, lpStudent->GetDBCameraID(), true);
    }
    // 将新的学生推流对象保存起来...
    m_lpStudentPusher = lpStudent;
    // 累加扩展音频变化次数(自动溢出回还)...
    this->AddExAudioChangeNum();
  } else if( idTag == ID_TAG_LOOKER ) {
    m_MapStudentLooker[nHostPort] = lpStudent;
  }
}
//
// 处理学生端删除事件...
void CRoom::doDeleteStudent(CStudent * lpStudent)
{
  // 如果输入的学生端对象无效，直接返回...
  if( lpStudent == NULL )
    return;
  // 如果是学生推流端 => 告诉房间里的TCP讲师端，可以删除拉流线程了...
  if( lpStudent == m_lpStudentPusher ) {
    // 如果是讲师端发起的删除，置空返回...
    if( lpStudent->GetDeleteByTCP() ) {
      m_lpStudentPusher = NULL;
      return;
    }
    // 如果是学生端发起的删除，通知讲师端，可以删除拉流线程了，然后置空，返回...
    GetApp()->doUDPStudentPusherOnLine(m_nRoomID, lpStudent->GetDBCameraID(), false);
    m_lpStudentPusher = NULL;
    return;
  }
  // 在学生观看端中遍历查找...
  int nHostPort = lpStudent->GetHostPort();
  GM_MapStudent::iterator itorItem = m_MapStudentLooker.find(nHostPort);
  // 找到相关观看学生端对象，直接删除返回...
  if( itorItem != m_MapStudentLooker.end() ) {
    m_MapStudentLooker.erase(itorItem);
    return;
  }
  // 如果通过端口方式没有找到，通过指针遍历查找...
  itorItem = m_MapStudentLooker.begin();
  while(itorItem != m_MapStudentLooker.end()) {
    // 找到了相关节点 => 删除节点，返回...
    if(itorItem->second == lpStudent) {
      m_MapStudentLooker.erase(itorItem);
      return;
    }
    // 2018.09.12 - by jackey => 造成过严重问题...
    // 没有找到相关节点 => 继续找下一个...
    ++itorItem;
  }
  // 通过指针遍历也没有找到，打印错误信息...
  log_trace("Can't find UDP-Student in CRoom, HostPort: %d", nHostPort);
}
//
// 根据传递过来的老师端类型，进行变量更新...
void CRoom::doCreateTeacher(CTeacher * lpTeacher)
{
  uint8_t idTag = lpTeacher->GetIdTag();
  if( idTag == ID_TAG_PUSHER ) {
    // 如果新的讲师推流对象与原有对象不相同(可能多次发命令) => 告诉所有TCP在线学生端，可以创建拉流线程了...
    if( m_lpTeacherPusher != lpTeacher ) {
      m_lpTeacherPusher = lpTeacher;
      GetApp()->doUDPTeacherPusherOnLine(m_nRoomID, true);
    }
    // 将新的讲师推流对象保存起来...
    m_lpTeacherPusher = lpTeacher;
  } else if( idTag == ID_TAG_LOOKER ) {
    // 先将新的老师观看者对象保存起来...
    m_lpTeacherLooker = lpTeacher;
    // 如果学生推流者不为空，需要开启探测...
    if( m_lpStudentPusher != NULL ) {
      m_lpStudentPusher->SetCanDetect(true);
    }
  }
}
//
// 处理老师端删除事件...
void CRoom::doDeleteTeacher(CTeacher * lpTeacher)
{
  // 如果是老师推流端 => 告诉所有TCP在线学生端，可以删除拉流线程了
  if( m_lpTeacherPusher == lpTeacher ) {
    GetApp()->doUDPTeacherPusherOnLine(m_nRoomID, false);
    m_lpTeacherPusher = NULL;
    return;
  }
  // 如果是老师观看端，通知学生推流端停止推流，置空，返回...
  if( m_lpTeacherLooker == lpTeacher ) {
    // 先将老师观看者对象置空...
    m_lpTeacherLooker = NULL;
    // 如果学生推流者不为空，需要关闭探测...
    if( m_lpStudentPusher != NULL ) {
      m_lpStudentPusher->SetCanDetect(false);
    }
    // 注意：这是第二种方案 => 直接发送停止推流 => 通道切换时会造成重复停止...
    //GetApp()->doUDPTeacherLookerDelete(m_nRoomID, lpTeacher->GetDBCameraID());
    return;
  }
}

// 处理老师推流者转发给学生观看者的数据包 => 转发给房间里所有学生观看者...
bool CRoom::doTeacherPusherToStudentLooker(char * lpBuffer, int inBufSize)
{
  if( lpBuffer == NULL || inBufSize <= 0 )
    return false;
  if( m_MapStudentLooker.size() <= 0 )
    return false;
  GM_MapStudent::iterator itorItem;
  for(itorItem = m_MapStudentLooker.begin(); itorItem != m_MapStudentLooker.end(); ++itorItem) {
    CStudent * lpStudent = itorItem->second;
    if( lpStudent == NULL ) continue;
    lpStudent->doTransferToFrom(lpBuffer, inBufSize);
  }
  return true;
}

// 处理学生推流者转发给学生观看者的数据包 => 目前只转发音频数据包...
// 注意：必须老师推流者和学生推流者同时有效时，才能进行这个特殊转发...
bool CRoom::doStudentPusherToStudentLooker(char * lpBuffer, int inBufSize)
{
  if( lpBuffer == NULL || inBufSize <= 0 )
    return false;
  // 将音频序列头信息追加到转发数据包头的保留字段当中...
  rtp_hdr_t * lpHdrHeader = (rtp_hdr_t*)lpBuffer;
  // 如果不是音频数据包，直接返回...
  if( lpHdrHeader->pt != PT_TAG_AUDIO )
    return false;
  // 如果没有在线的观看学生端，直接返回...
  if( m_MapStudentLooker.size() <= 0 )
    return false;
  // 如果学生推流者无效，直接返回...
  if( m_lpStudentPusher == NULL )
    return false;
  // 如果老师推流者无效，直接返回...
  if( m_lpTeacherPusher == NULL )
    return false;
  // 获取学生推流者的序列头信息 => 序列头为空，直接返回...
  string & strSeqHeader = m_lpStudentPusher->GetSeqHeader();
  if( strSeqHeader.size() <= 0 )
    return false;
  // 获取学生推流者自身的tcpSock标识符号...
  int nPushSockID = m_lpStudentPusher->GetTCPSockID();
  // 获取扩展音频变化次数(自动溢出回还)，保存到高两字节...
  lpHdrHeader->noset = ((uint32_t)(this->GetExAudioChangeNum()) << 16);
  // 获取序列头里面音频标识信息内容 => 获取第三个字节，填充到最低字节位...
  lpHdrHeader->noset |= (uint8_t)strSeqHeader.at(2);
  // 开始遍历学生观看者对象列表，转发学生推流者数据包...
  GM_MapStudent::iterator itorItem;
  for(itorItem = m_MapStudentLooker.begin(); itorItem != m_MapStudentLooker.end(); ++itorItem) {
    CStudent * lpStudent = itorItem->second;
    if (lpStudent == NULL) continue;
    // 如果推流者tcpSock与观看者tcpSock一致，不能转发给这个观看者...
    if (lpStudent->GetTCPSockID() == nPushSockID) continue;
    // 将学生推流者数据包转发给tcpSock不一致的学生观看者对象...
    lpStudent->doTransferToFrom(lpBuffer, inBufSize);
    // 打印扩展音频转发详细信息 => 只是为了调试使用...
    //log_debug("HostAddr: %u, HostPort: %d, ExAudio: %d", lpStudent->GetHostAddr(), lpStudent->GetHostPort(), lpHdrHeader->seq);
  }
  return true;
}
