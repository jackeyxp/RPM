
#pragma once

// define student role type...
enum ROLE_TYPE
{
	kRoleWanRecv   = 0,      // 外网接收者角色
	kRoleMultiRecv = 1,      // 组播接收者角色
	kRoleMultiSend = 2,      // 组播发送者角色
};

// define client type...
enum {
  kClientPHP       = 1,       // 网站端链接...
  kClientStudent   = 2,       // 学生端链接...
  kClientTeacher   = 3,       // 讲师端链接...
  kClientUdpServer = 4,       // UDP服务器...
  kClientScreen    = 5,       // 屏幕终端...
};

// define command id...
enum {
  kCmd_Student_Login        = 1,
  kCmd_Student_OnLine	      = 2,
  kCmd_Teacher_Login        = 3,
  kCmd_Teacher_OnLine       = 4,
  kCmd_UDP_Logout           = 5,
  kCmd_Camera_PullStart     = 6,
  kCmd_Camera_PullStop      = 7,
  kCmd_Camera_OnLineList    = 8,
  kCmd_Camera_LiveStart     = 9,
  kCmd_Camera_LiveStop      = 10,
  kCmd_Camera_PTZCommand    = 11,
  kCmd_UdpServer_Login      = 12,
  kCmd_UdpServer_OnLine     = 13,
  kCmd_UdpServer_AddTeacher = 14,
  kCmd_UdpServer_DelTeacher = 15,
  kCmd_UdpServer_AddStudent = 16,
  kCmd_UdpServer_DelStudent = 17,
  kCmd_PHP_GetUdpServer     = 18,
  kCmd_PHP_GetAllServer     = 19,
  kCmd_PHP_GetAllClient     = 20,
  kCmd_PHP_GetRoomList      = 21,
  kCmd_PHP_GetPlayerList    = 22,
  kCmd_PHP_Bind_Mini        = 23,
  kCmd_PHP_GetRoomFlow      = 24,
  kCmd_Camera_PusherID      = 25,
  kCmd_Screen_Login         = 26,
  kCmd_Screen_OnLine        = 27,
  kCmd_Screen_Packet        = 28,
  kCmd_Screen_Finish        = 29,
};

// define the command header...
typedef struct {
  int   m_pkg_len;    // body size...
  int   m_type;       // client type...
  int   m_cmd;        // command id...
  int   m_sock;       // php sock in transmit...
} Cmd_Header;

///////////////////////////////////////////////////////////
// Only for PHP transmit server...
//////////////////////////////////////////////////////////
typedef struct {
	char  pkg_len[8];  // body length, not including header
	char  cmd;         // command code
	char  status;      // status code for response
} TrackerHeader;

#define ERR_OK          0
#define ERR_NO_ROOM     10001
#define ERR_NO_SERVER   10002
#define ERR_MODE_MATCH  10003
#define ERR_NO_PARAM    10004
#define ERR_NO_TERMINAL 10005
#define ERR_TYPE_MATCH  10006
#define ERR_TIME_MATCH  10007
#define ERR_HAS_TEACHER 10008
