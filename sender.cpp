#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/wait.h>
#include <signal.h>

#include <cstdlib>
#include <cstring>
#include <iostream>
#include <stdio.h>
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
  // Bind socket.
  if (bind(sockfd, (struct sockaddr *) &sender_addr, sizeof(sender_addr)) < 0) {
    printf("ERROR: sender bind socket failure\n");
  }
  printf("server bind to port %d, done\n", sockfd);

  // Wait for retrieve message. 
  while (1) {
    memset(buf, 0, BUFSIZE);
    recvlen = recvfrom(sockfd, buf, BUFSIZE, 0, (struct sockaddr *)&rec_addr,
                       &addrlen);
    cout << "RECEIVE MSG: " << buf << endl;
 
    if (strncmp(buf, "RETRIEVE: ",10) == 0) {
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

  // sendwindow
  int cwnd = WINSIZE;
  int sendBase = 0;
  int nextSeq = 0;
  int lastSeq = -1; // EOF seq num.
  // sendqueue
  int ack[QSIZE];
  char sendQueue[QSIZE][BUFSIZE];
  memset(sendQueue, 0, QSIZE*BUFSIZE);
  memset(ack, 0, QSIZE);
  // select
  struct timeval tv;
  fd_set sockfds;
  // random for packet corrupt/loss.
  srand(time(NULL));

  // Send file.
  while (1) {
    // Unused slots exist in window.
    while ((((nextSeq >= sendBase) && (nextSeq - sendBase < cwnd)) ||
           ((nextSeq < sendBase) && ((QSIZE - sendBase) + nextSeq < cwnd)))
           && lastSeq == -1 ) { 

      // Read file.
      memset(buf, 0, SIZE);
      recvlen = fread(buf, 1, SIZE, fd);
      // Construct data. 
      char datagram[BUFSIZE];
      memset(datagram, 0, BUFSIZE); 
      struct SendHeader *header = (struct SendHeader *) datagram;
      // Write header.
      header->seqNum = nextSeq;
      header->EOFFlag = (feof(fd)) ? 1: 0;
      header->length = recvlen;
        if (feof(fd)) {
        lastSeq = nextSeq;
        fclose(fd);
      }
      // Write payload.
      memcpy(header->payload, buf, recvlen);

      // Write data to queue.
      memcpy(sendQueue[nextSeq], datagram, BUFSIZE);
      ack[nextSeq] = 0;

      // Send msg.
      if (sendto(sockfd, sendQueue[nextSeq], sizeof(SendHeader), 0,
          (struct sockaddr *)&rec_addr, sizeof(rec_addr)) < 0) {
        printf("Send msg to sender failed\n");
      } else {
        cout << "SEND: DATA " << nextSeq << endl;
      }

     // Increment sequence number. (circular)
      nextSeq = (nextSeq == QSIZE-1) ? 0: nextSeq + 1; 
    }

    // Wait for ACK.
    while (1) {
      // Set timeout
      tv.tv_sec = SEC;
      tv.tv_usec = USEC;
      // Set select fds.
      FD_ZERO(&sockfds);
      FD_SET(sockfd, &sockfds);

      // Use select to receive ack and deal with timeout.
      select(sockfd+1, &sockfds, NULL, NULL, &tv);

      // ACK received.
      if (FD_ISSET(sockfd, &sockfds)) {
        memset(buf, 0, BUFSIZE);
        recvlen = recvfrom(sockfd, buf, BUFSIZE, 0,
                           (struct sockaddr *)&rec_addr, &addrlen);

        struct ACKHeader *ackHeader = (struct ACKHeader *) buf;
        int ackNum = ackHeader->ackNum;
        
        // Deal with Loss and corrupt.
        if (isCorrupt()) {
          printf("\t\t\t%sCORRUPT: ACK %d\n%s", KRED, ackNum, KEND);
          continue;
        } else if (isLoss()) {
          printf("\t\t\t%sLOSS: ACK %d\n%s", KRED, ackNum, KEND); 
          continue;
        } else {
          ack[ackNum] = 1; // Set ACK.
          cout << "\t\t\tRECEIVE: ACK " << ackNum << endl;
        }
        
        // If ackNum != sendBase, continue to listen for ACK.
 
        // If ackNum == sendBase, update sendBase.
        if (ackNum == sendBase) {
          while (ack[sendBase]) {
            ack[sendBase] = 0; // clear ACK queue.
            // If ACK to the last seq, file has been reliably received.
            if (sendBase == lastSeq) {
              cout << "Reliable transfer done!" << endl;
              char msg[20] = "TRANSFER DONE";
              sendto(sockfd, msg, sizeof(msg), 0, (struct sockaddr *)&rec_addr,
                     sizeof(rec_addr));
              cout << "SEND: TRANSFER DONE (RECEIVE ALL ACK)" << endl;

              return 0;
            }
            sendBase = (sendBase == QSIZE-1)? 0: sendBase + 1;
          }
          // If sendBase is incremented, break while send more data.
          break;
        }
      } else { // Timeout. Resend base.
        // Resend pkt from queue.
        if (sendto(sockfd, sendQueue[sendBase], sizeof(SendHeader), 0,
                   (struct sockaddr *)&rec_addr, sizeof(rec_addr)) < 0) {
            printf("Send msg to sender failed\n");
        } else {
          printf("%sRESEND: DATA %d\n%s", KYEL, sendBase, KEND);
        }
      }    
    }
  }
  return 0;
}
