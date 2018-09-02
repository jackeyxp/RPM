
#pragma once

#define DEF_CENTER_ADDR    "edu.ihaoyi.cn"         // Ĭ��UDP���ķ���������...
#define DEF_CENTER_PORT             26026          // Ĭ��UDP���ķ������˿�...
#define DEF_TCP_PORT                21002          // Ĭ��TCP�������˿�
#define DEF_UDP_PORT                 5252          // Ĭ��UDP�������˿�
#define DEF_MTU_SIZE                  800          // Ĭ��MTU��Ƭ��С(�ֽ�)...
#define MAX_BUFF_LEN                 1024          // ����ĳ���(�ֽ�)...
#define MAX_OPEN_FILE                2048          // �����ļ������(��)...
#define MAX_EPOLL_SIZE               1024          // EPOLL�������ֵ...
#define MAX_LISTEN_SIZE              1024          // �����������ֵ...
#define CHECK_TIME_OUT                 10          // ��ʱ������� => ÿ��10�룬���һ�γ�ʱ...
#define PLAY_TIME_OUT                  15          // ���糬ʱ���� => 15��û�����ݣ���Ϊ��ʱ...
#define MAX_SLEEP_MS                   15          // �����Ϣʱ��(����)...
#define APP_SLEEP_MS                  100          // Ӧ�ò���Ϣʱ��(����)...
//
// ���彻���ն�����...
enum
{
  TM_TAG_STUDENT  = 0x01, // ѧ���˱��...
  TM_TAG_TEACHER  = 0x02, // ��ʦ�˱��...
  TM_TAG_SERVER   = 0x03, // ���������...
};
//
// ���彻���ն����...
enum
{
  ID_TAG_PUSHER  = 0x01,  // ��������� => ������...
  ID_TAG_LOOKER  = 0x02,  // ��������� => �ۿ���...
  ID_TAG_SERVER  = 0x03,  // ���������
};
//
// ����RTP�غ���������...
enum
{
  PT_TAG_DETECT  = 0x01,  // ̽��������...
  PT_TAG_CREATE  = 0x02,  // ����������...
  PT_TAG_DELETE  = 0x03,  // ɾ��������...
  PT_TAG_SUPPLY  = 0x04,  // ����������...
  PT_TAG_HEADER  = 0x05,  // ����Ƶ����ͷ...
  PT_TAG_READY   = 0x06,  // ׼����������...
  PT_TAG_AUDIO   = 0x08,  // ��Ƶ�� => FLV_TAG_TYPE_AUDIO...
  PT_TAG_VIDEO   = 0x09,  // ��Ƶ�� => FLV_TAG_TYPE_VIDEO...
  PT_TAG_LOSE    = 0x0A,  // �Ѷ�ʧ���ݰ�...
};
//
// ����̽�ⷽ������...
enum
{
  DT_TO_SERVER   = 0x01,  // ͨ����������ת��̽��
  DT_TO_P2P      = 0x02,  // ͨ���˵���ֱ�ӵ�̽��
};
//
// ����̽������ṹ�� => PT_TAG_DETECT
typedef struct {
  unsigned char   tm:2;         // terminate type => TM_TAG_STUDENT | TM_TAG_TEACHER
  unsigned char   id:2;         // identify type => ID_TAG_PUSHER | ID_TAG_LOOKER
  unsigned char   pt:4;         // payload type => PT_TAG_DETECT
  unsigned char   dtDir;        // ̽�ⷽ�� => DT_TO_SERVER | DT_TO_P2P
  unsigned short  dtNum;        // ̽�����
  unsigned int    tsSrc;        // ̽�ⷢ��ʱ��ʱ��� => ����
  unsigned int    maxAConSeq;   // ���ն����յ���Ƶ����������к� => ���߷��Ͷˣ��������֮ǰ����Ƶ���ݰ�������ɾ����
  unsigned int    maxVConSeq;   // ���ն����յ���Ƶ����������к� => ���߷��Ͷˣ��������֮ǰ����Ƶ���ݰ�������ɾ����
}rtp_detect_t;
//
// ���崴������ṹ�� => PT_TAG_CREATE
typedef struct {
  unsigned char   tm:2;         // terminate type => TM_TAG_STUDENT | TM_TAG_TEACHER
  unsigned char   id:2;         // identify type => ID_TAG_PUSHER | ID_TAG_LOOKER
  unsigned char   pt:4;         // payload type => PT_TAG_CREATE
  unsigned char   noset;        // ���� => �ֽڶ���
  unsigned short  liveID;       // ѧ��������ͷ���
  unsigned int    roomID;       // ���ҷ�����
  unsigned int    tcpSock;      // ������TCP�׽���
}rtp_create_t;
//
// ����ɾ������ṹ�� => PT_TAG_DELETE
typedef struct {
  unsigned char   tm:2;         // terminate type => TM_TAG_STUDENT | TM_TAG_TEACHER
  unsigned char   id:2;         // identify type => ID_TAG_PUSHER | ID_TAG_LOOKER
  unsigned char   pt:4;         // payload type => PT_TAG_DELETE
  unsigned char   noset;        // ���� => �ֽڶ���
  unsigned short  liveID;       // ѧ��������ͷ���
  unsigned int    roomID;       // ���ҷ�����
}rtp_delete_t;
//
// ���岹������ṹ�� => PT_TAG_SUPPLY
typedef struct {
  unsigned char   tm:2;         // terminate type => TM_TAG_STUDENT | TM_TAG_TEACHER
  unsigned char   id:2;         // identify type => ID_TAG_PUSHER | ID_TAG_LOOKER
  unsigned char   pt:4;         // payload type => PT_TAG_SUPPLY
  unsigned char   suType;       // �������� => 8(��Ƶ)9(��Ƶ)
  unsigned short  suSize;       // �������� / 4 = ��������
  // unsigned int => �������1
  // unsigned int => �������2
  // unsigned int => �������3
}rtp_supply_t;
//
// ��������Ƶ����ͷ�ṹ�� => PT_TAG_HEADER
typedef struct {
  unsigned char   tm:2;         // terminate type => TM_TAG_STUDENT | TM_TAG_TEACHER
  unsigned char   id:2;         // identify type => ID_TAG_PUSHER | ID_TAG_LOOKER
  unsigned char   pt:4;         // payload type => PT_TAG_HEADER
  unsigned char   hasAudio:4;   // �Ƿ�����Ƶ => 0 or 1
  unsigned char   hasVideo:4;   // �Ƿ�����Ƶ => 0 or 1
  unsigned char   rateIndex:5;  // ��Ƶ�������������
  unsigned char   channelNum:3; // ��Ƶͨ������
  unsigned char   fpsNum;       // ��Ƶfps��С
  unsigned short  picWidth;     // ��Ƶ���
  unsigned short  picHeight;    // ��Ƶ�߶�
  unsigned short  spsSize;      // sps����
  unsigned short  ppsSize;      // pps����
  // .... => sps data           // sps������
  // .... => pps data           // pps������
}rtp_header_t;
//
// ����׼����������ṹ�� => PT_TAG_READY
typedef struct {
  unsigned char   tm:2;         // terminate type => TM_TAG_STUDENT | TM_TAG_TEACHER
  unsigned char   id:2;         // identify type => ID_TAG_PUSHER | ID_TAG_LOOKER
  unsigned char   pt:4;         // payload type => PT_TAG_READY
  unsigned char   noset;        // ���� => �ֽڶ���
  unsigned short  recvPort;     // �����ߴ�͸�˿� => ���� => host
  unsigned int    recvAddr;     // �����ߴ�͸��ַ => ���� => host
}rtp_ready_t;
//
// ����RTP���ݰ�ͷ�ṹ�� => PT_TAG_AUDIO | PT_TAG_VIDEO
typedef struct {
  unsigned char   tm:2;         // terminate type => TM_TAG_STUDENT | TM_TAG_TEACHER
  unsigned char   id:2;         // identify type => ID_TAG_PUSHER | ID_TAG_LOOKER
  unsigned char   pt:4;         // payload type => PT_TAG_AUDIO | PT_TAG_VIDEO
  unsigned char   pk:4;         // payload is keyframe => 0 or 1
  unsigned char   pst:2;        // payload start flag => 0 or 1
  unsigned char   ped:2;        // payload end flag => 0 or 1
  unsigned short  psize;        // payload size => ������ͷ��������
  unsigned int    seq;          // rtp���к� => ��1��ʼ
  unsigned int    ts;           // ֡ʱ���  => ����
}rtp_hdr_t;
//
// ���嶪���ṹ��...
typedef struct {
  unsigned int    lose_seq;      // ��⵽�Ķ������к�
  unsigned int    resend_time;   // �ط�ʱ��� => cur_time + rtt_var => ����ʱ�ĵ�ǰʱ�� + ����ʱ�����綶��ʱ���
  unsigned short  resend_count;  // �ط�����ֵ => ��ǰ��ʧ�������ط�����
  unsigned char   lose_type;     // �������� => 8(��Ƶ)9(��Ƶ)
  unsigned char   noset;         // ���� => �ֽڶ���
}rtp_lose_t;
