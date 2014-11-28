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

#define BUFSIZE 1024
#define QSIZE 200
#define WINSIZE 4
#define SIZE 1016

#define LOSE 10
#define CORRUPT 10

struct SendHeader {
  uint32_t seqNum;
  uint16_t EOFFlag;
  uint16_t length;
  char payload[SIZE];
};

struct ACKHeader {
  uint32_t ackNum;
};

bool isCorrupt() {
  return (rand()%100 < CORRUPT) ? true : false;
}

bool isLoss() {
  return (rand()%100 < LOSE) ? true : false;
}
