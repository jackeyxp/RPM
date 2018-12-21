
#include "app.h"
#include "room.h"
#include "student.h"
#include "teacher.h"
#include "udpthread.h"

CUDPThread::CUDPThread()
  : m_next_detect_ns(-1)
  , m_next_check_ns(-1)
  , m_sem_t(NULL)
{
  // 初始化网络环形队列...
	circlebuf_init(&m_circle);
  // 初始化辅助线程信号量...
  os_sem_init(&m_sem_t, 0);
  // 初始化线程互斥对象...
  pthread_mutex_init(&m_buff_mutex, NULL);
  pthread_mutex_init(&m_room_mutex, NULL);
}

CUDPThread::~CUDPThread()
{
  // 等待线程退出...
  this->StopAndWaitForThread();
  // 先删终端，再删房间 => 终端有房间指针引用...
  this->clearAllClient();
  // 先删终端，再删房间 => 终端有房间指针引用...
  this->clearAllRoom();
	// 释放网络环形队列空间...
	circlebuf_free(&m_circle);
  // 释放辅助线程信号量...
  os_sem_destroy(m_sem_t);
  // 删除线程互斥对象...
  pthread_mutex_destroy(&m_buff_mutex);
  pthread_mutex_destroy(&m_room_mutex);
}

void CUDPThread::clearAllRoom()
{
  GM_MapRoom::iterator itorRoom;
  for(itorRoom = m_MapRoom.begin(); itorRoom != m_MapRoom.end(); ++itorRoom) {
    CRoom * lpRoom = itorRoom->second;
    delete lpRoom; lpRoom = NULL;
  }
}

void CUDPThread::clearAllClient()
{
  GM_MapNetwork::iterator itorItem;
  for(itorItem = m_MapNetwork.begin(); itorItem != m_MapNetwork.end(); ++itorItem) {
    CNetwork * lpNetwork = itorItem->second;
    delete lpNetwork; lpNetwork = NULL;
  }  
}

bool CUDPThread::InitThread()
{
  this->Start();
  return true;
}

bool CUDPThread::onRecvEvent(uint32_t inHostAddr, uint16_t inHostPort, char * lpBuffer, int inBufSize)
{
  // 如果线程已经处于退出状态，直接返回...
  if( this->IsStopRequested() )
    return true;
  // 线程进入互斥保护状态...
  pthread_mutex_lock(&m_buff_mutex);
  // 环形队列的数据结构 => uint32_t|uint16_t|int|char => HostAddr|HostPort|Size|Data
  circlebuf_push_back(&m_circle, &inHostAddr, sizeof(uint32_t));
  circlebuf_push_back(&m_circle, &inHostPort, sizeof(uint16_t));
  circlebuf_push_back(&m_circle, &inBufSize, sizeof(int));
  circlebuf_push_back(&m_circle, lpBuffer, inBufSize);
  // 线程退出互斥保护状态...
  pthread_mutex_unlock(&m_buff_mutex);
  // 通知线程信号量状态发生改变...
  os_sem_post(m_sem_t);
  return true;
}

void CUDPThread::Entry()
{
  // 设定默认的信号超时时间 => APP_SLEEP_MS 毫秒...
  unsigned long next_wait_ms = APP_SLEEP_MS;
  while( !this->IsStopRequested() ) {
    // 注意：这里用信号量代替sleep的目的是为了避免等待时的命令延时...
    // 无论信号量是超时还是被触发，都要执行下面的操作...
    //log_trace("[CUDPThread] start, sem-wait: %d", next_wait_ms);
    os_sem_timedwait(m_sem_t, next_wait_ms);
    //log_trace("[CUDPThread] end, sem-wait: %d", next_wait_ms);
    // 每隔 1 秒服务器向所有终端发起探测命令包...
    this->doSendDetectCmd();
    // 每隔 10 秒检测一次对象超时...
    this->doCheckTimeout();
    // 处理一个到达的UDP数据包...
    this->doRecvPacket();
    // 先发送针对讲师的补包命令...
    this->doSendSupplyCmd();
    // 再发送针对学生的丢包命令...
    this->doSendLoseCmd();
  }
  // 打印UDP线程退出信息...
  log_trace("udp-thread exit.");
  ////////////////////////////////////////////////////////////////////
  // 注意：房间对象和终端对象的删除，放在了析构函数当中...
  ////////////////////////////////////////////////////////////////////
}

// 遍历所有对象，发起探测命令...
void CUDPThread::doSendDetectCmd()
{
  // 每隔1秒发送一个探测命令包 => 必须转换成有符号...
  int64_t cur_time_ns = os_gettime_ns();
  int64_t period_ns = 1000 * 1000000;
  // 第一个探测包延时1/3秒发送...
  if( m_next_detect_ns < 0 ) { 
    m_next_detect_ns = cur_time_ns + period_ns / 3;
  }
  // 如果发包时间还没到，直接返回...
  if( m_next_detect_ns > cur_time_ns )
    return;
  assert( cur_time_ns >= m_next_detect_ns );
  // 计算下次发送探测命令的时间戳...
  m_next_detect_ns = os_gettime_ns() + period_ns;
  // 线程互斥锁定...
  pthread_mutex_lock(&m_room_mutex);  
  // 遍历终端队列，进行网络探测...
  CNetwork * lpNetwork = NULL;
  GM_MapNetwork::iterator itorItem = m_MapNetwork.begin();
  while( itorItem != m_MapNetwork.end() ) {
    // 调用发送探测命令，忽略结果...
    lpNetwork = itorItem->second;
    lpNetwork->doServerSendDetect();
    ++itorItem;
  }
  // 线程互斥解锁...
  pthread_mutex_unlock(&m_room_mutex);
}

// 遍历对象，进行超时检测...
void CUDPThread::doCheckTimeout()
{
  // 每隔 10 秒检测一次对象超时 => 加上LL防止整数溢出...
  int64_t period_ns = 10 * 1000 * 1000000LL;
  int64_t cur_time_ns = os_gettime_ns();
  // 设定第一个初始值的情况...
  if( m_next_check_ns < 0 ) {
    m_next_check_ns = cur_time_ns + period_ns;
  }
  // 如果检测时间还没到，直接返回...
  if( m_next_check_ns > cur_time_ns )
  	return;
  assert( cur_time_ns >= m_next_check_ns );
  // 计算下次进行超时检测命令的时间戳...
  m_next_check_ns = os_gettime_ns() + period_ns;
  // 线程互斥锁定...
  pthread_mutex_lock(&m_room_mutex);  
  // 遍历终端，进行超时检测...
  CNetwork * lpNetwork = NULL;
  GM_MapNetwork::iterator itorItem = m_MapNetwork.begin();
  while( itorItem != m_MapNetwork.end() ) {
    lpNetwork = itorItem->second;
    if( lpNetwork->IsTimeout() ) {
      // 在析构函数中对房间信息进行清理工作...
      delete lpNetwork; lpNetwork = NULL;
      m_MapNetwork.erase(itorItem++);
    } else {
      ++itorItem;
    }
  }
  // 线程互斥解锁...
  pthread_mutex_unlock(&m_room_mutex);
}

void CUDPThread::doRecvPacket()
{
  // 准备接受数据块变量...
  uint32_t inHostAddr  = 0;
  uint16_t inHostPort  = 0;
  int      inBufSize   = 0;
  bool     bCanSemPost = false;
  char     recvBuff[MAX_BUFF_LEN] = {0};
  // 线程进入互斥保护状态...
  pthread_mutex_lock(&m_buff_mutex);
  // 环形队列有数据才处理...
  if( m_circle.size > 0 ) {
    // 从环形队列读取一个完整数据块 => uint32_t|uint16_t|int|char => HostAddr|HostPort|Size|Data
    circlebuf_pop_front(&m_circle, &inHostAddr, sizeof(uint32_t));
    circlebuf_pop_front(&m_circle, &inHostPort, sizeof(uint16_t));
    circlebuf_pop_front(&m_circle, &inBufSize, sizeof(int));
    circlebuf_pop_front(&m_circle, recvBuff, inBufSize);
    bCanSemPost = ((m_circle.size > 0 ) ? true : false);
  }
  // 线程退出互斥保护状态...
  pthread_mutex_unlock(&m_buff_mutex);
  // 处理网络数据到达事件 => 数据块有效才处理...
  if( inBufSize > 0 && inHostAddr > 0 && inHostPort > 0 ) {
    pthread_mutex_lock(&m_room_mutex);
    this->doProcSocket(inHostAddr, inHostPort, recvBuff, inBufSize);
    pthread_mutex_unlock(&m_room_mutex);
  }
  // 环形队列还有数据，改变信号量，再次触发...
  bCanSemPost ? os_sem_post(m_sem_t) : NULL;
}

void CUDPThread::doSendSupplyCmd()
{
  // 线程互斥锁定...
  pthread_mutex_lock(&m_room_mutex);
  // 补包发送结果 => -1(删除)0(没发)1(已发)...
  GM_ListTeacher::iterator itorItem = m_ListTeacher.begin();
  while( itorItem != m_ListTeacher.end() ) {
    // 执行补包命令，返回执行结果...
    int nSendResult = (*itorItem)->doServerSendSupply();
    // -1 => 没有补包了，从列表中删除...
    if( nSendResult < 0 ) {
      m_ListTeacher.erase(itorItem++);
      continue;
    }
    // 继续检测下一个有补包的老师推流端...
    ++itorItem;
    // 0 => 有补包，但是不到补包时间 => 休息15毫秒...
    if( nSendResult == 0 )
      continue;
    // 1 => 有补包，已经发送补包命令 => 不要休息...
    os_sem_post(m_sem_t);
  }
  // 线程互斥解锁...
  pthread_mutex_unlock(&m_room_mutex);
}

void CUDPThread::doSendLoseCmd()
{
  // 线程互斥锁定...
  pthread_mutex_lock(&m_room_mutex);
  // 遍历包含丢包数据学生队列对象...
  GM_ListStudent::iterator itorItem = m_ListStudent.begin();
  while( itorItem != m_ListStudent.end() ) {
    // 执行发送丢包数据内容，返回是否还要执行丢包...
    bool bSendResult = (*itorItem)->doServerSendLose();
    // false => 没有丢包要发了，从队列当中删除...
    if( !bSendResult ) {
      m_ListStudent.erase(itorItem++);
      continue;
    }
    assert( bSendResult );
    // true => 还有丢包要发送，不能休息...
    ++itorItem; os_sem_post(m_sem_t);
  }
  // 线程互斥解锁...
  pthread_mutex_unlock(&m_room_mutex);
}

bool CUDPThread::doProcSocket(uint32_t nHostSinAddr, uint16_t nHostSinPort, char * lpBuffer, int inBufSize)
{
  // 通过第一个字节的低2位，判断终端类型...
  uint8_t tmTag = lpBuffer[0] & 0x03;
  // 获取第一个字节的中2位，得到终端身份...
  uint8_t idTag = (lpBuffer[0] >> 2) & 0x03;
  // 获取第一个字节的高4位，得到数据包类型...
  uint8_t ptTag = (lpBuffer[0] >> 4) & 0x0F;

  //////////////////////////////////////////////////////////////////////////////////////////////////////////////
  // 打印调试信息 => 打印所有接收到的数据包内容格式信息...
  //log_debug("recvfrom, size: %u, tmTag: %d, idTag: %d, ptTag: %d", inBufSize, tmTag, idTag, ptTag);
  //////////////////////////////////////////////////////////////////////////////////////////////////////////////

  // 如果终端既不是Student也不是Teacher或Server，错误终端，直接扔掉数据...
  if( tmTag != TM_TAG_STUDENT && tmTag != TM_TAG_TEACHER && tmTag != TM_TAG_SERVER ) {
    log_debug("Error Terminate Type: %d", tmTag);
    return false;
  }
  // 如果终端身份既不是Pusher也不是Looker或Server，错误身份，直接扔掉数据...
  if( idTag != ID_TAG_PUSHER && idTag != ID_TAG_LOOKER && idTag != ID_TAG_SERVER ) {
    log_debug("Error Identify Type: %d", idTag);
    return false;
  }
  // 如果是删除指令，需要做特殊处理...
  if( ptTag == PT_TAG_DELETE ) {
    this->doTagDelete(nHostSinPort);
    return false;
  }
  // 通过获得的端口，查找CNetwork对象...
  CNetwork * lpNetwork = NULL;
  GM_MapNetwork::iterator itorItem;
  itorItem = m_MapNetwork.find(nHostSinPort);
  // 如果没有找到创建一个新的对象...
  if( itorItem == m_MapNetwork.end() ) {
    // 如果不是创建命令 => 打印错误信息...
    if( ptTag != PT_TAG_CREATE ) {
      log_debug("Server Reject for tmTag: %s, idTag: %s, ptTag: %d", get_tm_tag(tmTag), get_id_tag(idTag), ptTag);
      return false;
    }
    assert( ptTag == PT_TAG_CREATE );
    // 只有创建命令可以被用来创建新对象...
    switch( tmTag ) {
      case TM_TAG_STUDENT: lpNetwork = new CStudent(this, tmTag, idTag, nHostSinAddr, nHostSinPort); break;
      case TM_TAG_TEACHER: lpNetwork = new CTeacher(this, tmTag, idTag, nHostSinAddr, nHostSinPort); break;
    }
  } else {
    // 注意：这里可能会连续收到 PT_TAG_CREATE 命令，不影响...
    lpNetwork = itorItem->second;
    assert( lpNetwork->GetHostAddr() == nHostSinAddr );
    assert( lpNetwork->GetHostPort() == nHostSinPort );
    // 注意：探测包的tmTag和idTag，可能与对象的tmTag和idTag不一致 => 探测包可能是转发的，身份是相反的...
  }
  // 如果网络层对象为空，打印错误...
  if( lpNetwork == NULL ) {
    log_trace("Error CNetwork is NULL");
    return false;
  }
  // 将网络对象更新到对象集合当中...
  m_MapNetwork[nHostSinPort] = lpNetwork;
  // 将获取的数据包投递到网络对象当中...
  return lpNetwork->doProcess(ptTag, lpBuffer, inBufSize);
}

void CUDPThread::doAddSupplyForTeacher(CTeacher * lpTeacher)
{
  // 注意：是从doProcSocket()过来的，已经做了互斥处理...
  GM_ListTeacher::iterator itorItem;
  itorItem = std::find(m_ListTeacher.begin(), m_ListTeacher.end(), lpTeacher);
  // 如果对象已经存在列表当中，直接返回...
  if( itorItem != m_ListTeacher.end() )
    return;
  // 对象没有在列表当中，放到列表尾部...
  m_ListTeacher.push_back(lpTeacher);
}

void CUDPThread::doDelSupplyForTeacher(CTeacher * lpTeacher)
{
  // 注意：是从doProcSocket()过来的，已经做了互斥处理...
  m_ListTeacher.remove(lpTeacher);
}

void CUDPThread::doAddLoseForStudent(CStudent * lpStudent)
{
  // 注意：是从doProcSocket()过来的，已经做了互斥处理...
  GM_ListStudent::iterator itorItem;
  itorItem = std::find(m_ListStudent.begin(), m_ListStudent.end(), lpStudent);
  // 如果对象已经存在列表当中，直接返回...
  if( itorItem != m_ListStudent.end() )
    return;
  // 对象没有在列表当中，放到列表尾部...
  m_ListStudent.push_back(lpStudent);
}

void CUDPThread::doDelLoseForStudent(CStudent * lpStudent)
{
  // 注意：是从doProcSocket()过来的，已经做了互斥处理...
  m_ListStudent.remove(lpStudent);
}

// 创建房间 => 通过房间号进行创建...
CRoom * CUDPThread::doCreateRoom(int inRoomID)
{
  // 注意：是从doProcSocket()过来的，已经做了互斥处理...
  // 如果找到了房间对象，直接返回...
  CRoom * lpRoom = NULL;
  GM_MapRoom::iterator itorRoom = m_MapRoom.find(inRoomID);
  if( itorRoom != m_MapRoom.end() ) {
    lpRoom = itorRoom->second;
  } else {
    // 如果没有找到房间，创建一个新的房间...
    lpRoom = new CRoom(inRoomID);
    m_MapRoom[inRoomID] = lpRoom;
  }
  return lpRoom;
}

void CUDPThread::doTagDelete(int nHostPort)
{
  /////////////////////////////////////////////////////////////////////
  // 注意：有两处调用这个删除接口，两处都在调用处进行了互斥保护...
  // 接口：doDeleteForCameraLiveStop()
  // 接口：doProcSocket()
  /////////////////////////////////////////////////////////////////////
  // 通过获得的端口，查找CNetwork对象...
  CNetwork * lpNetwork = NULL;
  GM_MapNetwork::iterator itorItem;
  itorItem = m_MapNetwork.find(nHostPort);
  if( itorItem == m_MapNetwork.end() ) {
    log_debug("Delete can't find CNetwork by host port(%d)", nHostPort);
    return;
  }
  // 将找到的CNetwork对象删除之...
  lpNetwork = itorItem->second;
  delete lpNetwork; lpNetwork = NULL;
  m_MapNetwork.erase(itorItem++);  
}

bool CUDPThread::IsUDPStudentPusherOnLine(int inRoomID, int inDBCameraID)
{
  // 首先进行线程互斥...
  pthread_mutex_lock(&m_room_mutex);
  bool bOnLine = false;
  CRoom * lpRoom = NULL;
  CStudent * lpStudent = NULL;
  GM_MapRoom::iterator itorRoom;
  do {
    itorRoom = m_MapRoom.find(inRoomID);
    if( itorRoom == m_MapRoom.end() )
      break;
    lpRoom = itorRoom->second; assert(lpRoom != NULL);
    lpStudent = lpRoom->GetStudentPusher();
    // 房间里没有学生推流者...
    if( lpStudent == NULL )
      break;
    // 检测学生推流者的通道编号是否与输入的通道编号一致...
    bOnLine = ((lpStudent->GetDBCameraID() == inDBCameraID) ? true : false);
  } while( false );
  // 退出互斥，返回查找结果...
  pthread_mutex_unlock(&m_room_mutex);  
  return bOnLine;  
}

bool CUDPThread::IsUDPTeacherPusherOnLine(int inRoomID)
{
  // 首先进行线程互斥...
  pthread_mutex_lock(&m_room_mutex);
  bool bOnLine = false;
  CRoom * lpRoom = NULL;
  GM_MapRoom::iterator itorRoom;
  do {
    itorRoom = m_MapRoom.find(inRoomID);
    if( itorRoom == m_MapRoom.end() )
      break;
    lpRoom = itorRoom->second; assert(lpRoom != NULL);
    bOnLine = ((lpRoom->GetTeacherPusher() != NULL) ? true : false);
  } while( false );
  // 退出互斥，返回查找结果...
  pthread_mutex_unlock(&m_room_mutex);  
  return bOnLine;
}

// 处理由kCmd_Camera_LiveStop触发的删除事件...
void CUDPThread::doDeleteForCameraLiveStop(int inRoomID)
{
  // 首先进行线程互斥...
  pthread_mutex_lock(&m_room_mutex);
  CStudent * lpStudentPusher = NULL;
  CTeacher * lpTeacherLooker = NULL;
  CRoom    * lpUdpRoom = NULL;
  do {
    // 通过指定的房间编号查找UDP房间对象...
    GM_MapRoom::iterator itorRoom = m_MapRoom.find(inRoomID);
    if( itorRoom == m_MapRoom.end() )
      break;
    // 获取UDP房间里的唯一老师观看者对象和唯一学生推流者对象...
    lpUdpRoom = itorRoom->second; assert(lpUdpRoom != NULL);
    lpTeacherLooker = lpUdpRoom->GetTeacherLooker();
    lpStudentPusher = lpUdpRoom->GetStudentPusher();
    // 房间里的老师观看者对象有效，并且UDP删除命令还没有到达，删除之...
    if( lpTeacherLooker != NULL && !lpTeacherLooker->GetDeleteByUDP() ) {
      // 设置删除标志，不要通知老师端，避免死锁...
      lpTeacherLooker->SetDeleteByTCP();
      // 通过关联端口号，删除老师观看者对象...
      this->doTagDelete(lpTeacherLooker->GetHostPort());
    }
    // 房间里的学生推流者对象有效，并且UDP删除命令还没有到达，删除之...
    if( lpStudentPusher != NULL && !lpStudentPusher->GetDeleteByUDP() ) {
      // 设置删除标志，不要通知学生端，避免死锁...
      lpStudentPusher->SetDeleteByTCP();
      // 通过关联端口号，删除学生推流者对象...
      this->doTagDelete(lpStudentPusher->GetHostPort());
    }
  } while( false );  
  // 最后退出线程互斥...
  pthread_mutex_unlock(&m_room_mutex);    
}
