#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/wait.h>
#include <signal.h>

#include <cstdlib>
#include <cstring>
#include <iostream>
#include <stdio.h>
#include <vector>

#include <sys/time.h>

#include "header.h"
using namespace std;

int main (int argc, char* argv[])
{
  struct sockaddr_in sender_addr; // sender address
  struct sockaddr_in rec_addr; // receiver address
  socklen_t addrlen = sizeof(rec_addr);

  int sockfd; // server socket
  int port;

  char buf[BUFSIZE]; // receive buffer
  int recvlen; // # bytes received

  FILE *fd;
  char filename[256];

  // Make sure the port number is provided.   
  if (argc < 2) {
    fprintf(stderr,"USAGE: %s PORT\n", argv[0]);
    exit(1);
  }

  // Obtain the fd for UDP listening socket.
  sockfd = socket(AF_INET, SOCK_DGRAM, 0);
  if (sockfd < 0) {
    printf("ERROR: sender open socket failure\n");
  }
  printf("Sender socket created\n");

  port = atoi(argv[1]);  
  memset((char*) &sender_addr, 0, sizeof(sender_addr)) ;
  sender_addr.sin_family = AF_INET;
  sender_addr.sin_addr.s_addr = INADDR_ANY;
  sender_addr.sin_port = htons(port);

  if (bind(sockfd, (struct sockaddr *) &sender_addr, sizeof(sender_addr)) < 0) {
    printf("ERROR: sender bind socket failure\n");
  }
  printf("server bind to port %d, done\n", sockfd);

  // Wait for retrieve message. 
  while (1) {
    memset(buf, 0, BUFSIZE);
    recvlen = recvfrom(sockfd, buf, BUFSIZE, 0, (struct sockaddr *)&rec_addr, &addrlen);
    cout << "RECEIVE MSG: " << buf << endl;
 
    if (strncmp(buf, "retrieve: ",10) == 0) {
      // Check if file exists.
      strcpy(filename, buf+10);
      if ((fd = fopen(filename, "rb")) == NULL) {
        cout << "File " << filename << " does not exist!" << endl;
      } else {
        cout << "OPEN FILE: " << filename << endl;
        break;
      }
    }
  }

  // sendqueue
  int ack[QUEUESIZE];
  char sendQueue[QUEUESIZE][BUFSIZE];
  memset(sendQueue, 0, QUEUESIZE*BUFSIZE);
  int winSize = WINSIZE;
  int sendBase = 0;
  int nextSeq = 0;
  int lastSeq = -1; // EOF seq num.

  // select
  struct timeval tv;
  fd_set sockfds;

  // Send file.
  while (1) {
    while ((nextSeq < sendBase + winSize) && lastSeq == -1) {
      // More slots in window.
      // Read file.
      char payload[SIZE];
      memset(payload, 0, SIZE);
      int n = fread(payload, 1, SIZE, fd);
 
      char datagram[BUFSIZE];
      memset(datagram, 0, BUFSIZE); 
      struct SendHeader *header = (struct SendHeader *) datagram;
      // Write header.
      header->seqNum = nextSeq;
      header->EOFFlag = (feof(fd)) ? 1: 0;
      header->length = n;
        if (feof(fd)) {
        lastSeq = nextSeq;
        fclose(fd);
      }
      // Write payload.
      memcpy(header->payload, payload, n);

      // Write queue. 
      memcpy(sendQueue[nextSeq], datagram, BUFSIZE);
      ack[nextSeq] = 0;

#ifdef DEBUG
      cout << "Seq #:" << header->nextSeq << " EOF: " << header->EOFFlag << " length: " << header->length << endl;
#endif

      // Send msg.
      if (sendto(sockfd, sendQueue[nextSeq], sizeof(SendHeader), 0, (struct sockaddr *)&rec_addr, sizeof(rec_addr)) < 0) {
        printf("Send msg to sender failed\n");
      } else {
        cout << "SEND: PKT " << nextSeq << endl;
      }

     // Increment sequence number.
      nextSeq = nextSeq + 1; 
    }

    // Wait for ACK.
    int update = 0;
    while (!update) {
      // Set timeout 2.5s
      tv.tv_sec = 2;
      tv.tv_usec = 500000;
      FD_ZERO(&sockfds);
      FD_SET(sockfd, &sockfds);

      // Use select for timeout.
      select(sockfd+1, &sockfds, NULL, NULL, &tv);

      if (FD_ISSET(sockfd, &sockfds)) { // Receive ACK.
        memset(buf, 0, BUFSIZE);
        recvlen = recvfrom(sockfd, buf, BUFSIZE, 0, (struct sockaddr *)&rec_addr, &addrlen);

        struct ACKHeader *ackHeader = (struct ACKHeader *) buf;
        int ackNum = ackHeader->ackNum;
        // Set ACK.
        ack[ackNum] = 1;
        cout << "RECEIVE: ACK " << ackNum << endl;

#ifdef DEBUG
        cout << "SendBase = " << sendBase << endl;
        cout << "lastSeq = " << lastSeq << endl;
#endif
        // If ackNum != sendBase, continue to listen for ACK.
 
        // If ackNum == sendBase, update sendBase
        if (ackNum == sendBase) {
          while (ack[sendBase]) {
            ack[sendBase] = 0; // clear ACK queue.
            // If ACK to the last seq, file has been reliably received.
            if (sendBase == lastSeq) {
              cout << "Reliable transfer done!" << endl;
              return 0;
            }
            sendBase = sendBase + 1;
          }
          // Set update to 1 to send more pkt.
          update = 1;
        }
      } else { // Timeout. Resend base.
        // Resend pkt from queue.
        if (sendto(sockfd, sendQueue[sendBase], sizeof(SendHeader), 0, (struct sockaddr *)&rec_addr, sizeof(rec_addr)) < 0) {
            printf("Send msg to sender failed\n");
        } else {
          cout << "RESEND: PKT " << sendBase << endl;
        }
      }    
    }

  }
  return 0;
}
