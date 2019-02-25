
#pragma once

// define student role type...
enum ROLE_TYPE
{
	kRoleWanRecv   = 0,      // ���������߽�ɫ
	kRoleMultiRecv = 1,      // �鲥�����߽�ɫ
	kRoleMultiSend = 2,      // �鲥�����߽�ɫ
};

// define client type...
enum {
  kClientPHP       = 1,       // ��վ������...
  kClientStudent   = 2,       // ѧ��������...
  kClientTeacher   = 3,       // ��ʦ������...
  kClientUdpServer = 4,       // UDP������...
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
};

// define the command header...
typedef struct {
  int   m_pkg_len;    // body size...
  int   m_type;       // client type...
  int   m_cmd;        // command id...
  int   m_sock;       // php sock in transmit...
} Cmd_Header;
