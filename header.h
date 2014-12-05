#include <time.h>

//printf color
#define KNRM  "\x1B[0m"
#define KRED  "\x1B[31m"
#define KGRN  "\x1B[32m"
#define KYEL  "\x1B[33m"
#define KBLU  "\x1B[34m"
#define KMAG  "\x1B[35m"
#define KCYN  "\x1B[36m"
#define KWHT  "\x1B[37m"
#define KEND  "\x1B[0m"
// Flag
#define RET 0x10
#define SYN 0x8
#define ACK 0x4
#define FIN 0x2
#define END 0x1
// Queue
#define BUFSIZE 1024
#define QSIZE 10
#define WINSIZE 4
#define SIZE 1012
// Packet lost/corrupt rate
#define LOSE 10
#define CORRUPT 10
// Timeout 1.5s
#define SEC 1
#define USEC 50000

/* Sender packet format */
struct Header {
  uint32_t seqNum;
  uint32_t ackNum;
  uint16_t flag;
  uint16_t length;
  char payload[SIZE];
};

bool isCorrupt() {
  return (rand()%100 < CORRUPT) ? true : false;
}

bool isLoss() {
  return (rand()%100 < LOSE) ? true : false;
}
