#include <time.h>
#include <sys/time.h>
//printf color
#define KRED  "\x1B[31m"
#define KGRN  "\x1B[32m"
#define KYEL  "\x1B[33m"
#define KEND  "\x1B[0m"
// Flag
#define SYN 0x8
#define ACK 0x4
#define FIN 0x2
#define END 0x1
// Queue
#define BUFSIZE 1024
#define QSIZE 100
#define WINSIZE 4
#define SIZE 1016
// Packet lost/corrupt rate
#define LOSE 20
#define CORRUPT 20
// Timeout 1.5s
#define SEC 0
#define USEC 50000

/* Sender packet format */
struct Header {
  uint32_t seqNum;
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

void timestamp()
{
  timeval tp;
  gettimeofday(&tp, 0);
  time_t curtime = tp.tv_sec;
  tm *t = localtime(&curtime);
  printf("%02d:%02d:%02d:%03ld   ", t->tm_hour, t->tm_min, t->tm_sec, tp.tv_usec/1000);
}
