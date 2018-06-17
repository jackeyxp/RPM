
#pragma once

#define DEF_UDP_HOME        "edu.ihaoyi.cn" // 默认UDP服务器地址
#define DEF_UDP_PORT        5252            // 默认UDP服务器端口
#define DEF_MTU_SIZE        800             // 默认MTU分片大小
#define MAX_BUFF_LEN        1024            // 最大报文长度...
#define LOG_MAX_SIZE        2048            // 单条日志最大长度...
#define MAX_OPEN_FILE       2048            // 最大打开文件句柄数...
#define CHECK_TIME_OUT        10            // 超时检测周期 => 每隔10秒，检测一次超时...
#define PLAY_TIME_OUT         15            // 网络超时周期 => 15秒没有数据，认为超时...

//
// 定义交互终端类型...
enum
{
  TM_TAG_STUDENT  = 0x01, // 学生端标记...
  TM_TAG_TEACHER  = 0x02, // 讲师端标记...
};
//
// 定义交互终端身份...
enum
{
  ID_TAG_PUSHER  = 0x01,  // 推流者身份 => 发送者...
  ID_TAG_LOOKER  = 0x02,  // 拉流者身份 => 观看者...
};
//
// 定义RTP载荷命令类型...
enum
{
  PT_TAG_DETECT  = 0x01,  // 探测命令标记...
  PT_TAG_CREATE  = 0x02,  // 创建命令标记...
  PT_TAG_DELETE  = 0x03,  // 删除命令标记...
  PT_TAG_SUPPLY  = 0x04,  // 补包命令标记...
  PT_TAG_HEADER  = 0x05,  // 音视频序列头...
  PT_TAG_READY   = 0x06,  // 准备继续命令...
  PT_TAG_AUDIO   = 0x08,  // 音频包 => FLV_TAG_TYPE_AUDIO...
  PT_TAG_VIDEO   = 0x09,  // 视频包 => FLV_TAG_TYPE_VIDEO...
};
//
// 定义探测命令结构体 => PT_TAG_DETECT
typedef struct {
  unsigned char   tm:2;         // terminate type => TM_TAG_STUDENT | TM_TAG_TEACHER
  unsigned char   id:2;         // identify type => ID_TAG_PUSHER | ID_TAG_LOOKER
  unsigned char   pt:4;         // payload type => PT_TAG_DETECT
  unsigned char   noset;        // 保留 => 字节对齐
  unsigned short  dtNum;        // 探测序号
  unsigned int    tsSrc;        // 探测发起时的时间戳 => 毫秒
}rtp_detect_t;
//
// 定义创建命令结构体 => PT_TAG_CREATE
typedef struct {
  unsigned char   tm:2;         // terminate type => TM_TAG_STUDENT | TM_TAG_TEACHER
  unsigned char   id:2;         // identify type => ID_TAG_PUSHER | ID_TAG_LOOKER
  unsigned char   pt:4;         // payload type => PT_TAG_CREATE
  unsigned char   noset;        // 保留 => 字节对齐
  unsigned short  liveID;       // 学生端摄像头编号
  unsigned int    roomID;       // 教室房间编号
}rtp_create_t;
//
// 定义删除命令结构体 => PT_TAG_DELETE
typedef struct {
  unsigned char   tm:2;         // terminate type => TM_TAG_STUDENT | TM_TAG_TEACHER
  unsigned char   id:2;         // identify type => ID_TAG_PUSHER | ID_TAG_LOOKER
  unsigned char   pt:4;         // payload type => PT_TAG_DELETE
  unsigned char   noset;        // 保留 => 字节对齐
  unsigned short  liveID;       // 学生端摄像头编号
  unsigned int    roomID;       // 教室房间编号
}rtp_delete_t;
//
// 定义补包命令结构体 => PT_TAG_SUPPLY
typedef struct {
  unsigned char   tm:2;         // terminate type => TM_TAG_STUDENT | TM_TAG_TEACHER
  unsigned char   id:2;         // identify type => ID_TAG_PUSHER | ID_TAG_LOOKER
  unsigned char   pt:4;         // payload type => PT_TAG_SUPPLY
  unsigned char   noset;        // 保留 => 字节对齐
  unsigned short  suSize;       // 补报长度 / 4 = 补包个数
  // unsigned int => 补包序号1
  // unsigned int => 补包序号2
  // unsigned int => 补包序号3
}rtp_supply_t;
//
// 定义音视频序列头结构体 => PT_TAG_HEADER
typedef struct {
  unsigned char   tm:2;         // terminate type => TM_TAG_STUDENT | TM_TAG_TEACHER
  unsigned char   id:2;         // identify type => ID_TAG_PUSHER | ID_TAG_LOOKER
  unsigned char   pt:4;         // payload type => PT_TAG_HEADER
  unsigned char   hasAudio:4;   // 是否有音频 => 0 or 1
  unsigned char   hasVideo:4;   // 是否有视频 => 0 or 1
  unsigned char   rateIndex:5;  // 音频采样率索引编号
  unsigned char   channelNum:3; // 音频通道数量
  unsigned char   fpsNum;       // 视频fps大小
  unsigned short  picWidth;     // 视频宽度
  unsigned short  picHeight;    // 视频高度
  unsigned short  spsSize;      // sps长度
  unsigned short  ppsSize;      // pps长度
  // .... => sps data           // sps数据区
  // .... => pps data           // pps数据区
}rtp_header_t;
//
// 定义准备就绪命令结构体 => PT_TAG_READY
typedef struct {
  unsigned char   tm:2;         // terminate type => TM_TAG_STUDENT | TM_TAG_TEACHER
  unsigned char   id:2;         // identify type => ID_TAG_PUSHER | ID_TAG_LOOKER
  unsigned char   pt:4;         // payload type => PT_TAG_READY
  unsigned char   noset;        // 保留 => 字节对齐
  unsigned short  recvPort;     // 接收者穿透端口 => 备用 => host
  unsigned int    recvAddr;     // 接收者穿透地址 => 备用 => host
}rtp_ready_t;
//
// 定义RTP数据包头结构体 => PT_TAG_AUDIO | PT_TAG_VIDEO
typedef struct {
  unsigned char   tm:2;         // terminate type => TM_TAG_STUDENT | TM_TAG_TEACHER
  unsigned char   id:2;         // identify type => ID_TAG_PUSHER | ID_TAG_LOOKER
  unsigned char   pt:4;         // payload type => PT_TAG_AUDIO | PT_TAG_VIDEO
  unsigned char   pk:4;         // payload is keyframe => 0 or 1
  unsigned char   pst:2;        // payload start flag => 0 or 1
  unsigned char   ped:2;        // payload end flag => 0 or 1
  unsigned short  psize;        // payload size => 不含包头，纯数据
  unsigned int    seq;          // rtp序列号 => 从1开始，第一个必须是 PT_TAG_HEADER
  unsigned int    ts;           // 帧时间戳  => 毫秒
}rtp_hdr_t;
