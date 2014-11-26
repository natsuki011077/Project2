#define BUFSIZE 1024
#define QUEUESIZE 500
#define WINSIZE 10
#define SIZE 1016

struct SendHeader {
  uint32_t seqNum;
  uint16_t EOFFlag;
  uint16_t length;
  char payload[SIZE];
};

struct ACKHeader {
  uint32_t ackNum;
};

