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

  int recvlen; // # bytes received
  char buf[BUFSIZE]; // receive buffer
  char datagram[BUFSIZE]; //sender buffer
  // header parser pointer for receive data
  struct Header *recvHeader = (struct Header *) buf;
  // header parser pointer for send data
  struct Header *sendHeader = (struct Header *) datagram;

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

  // sendwindow
  int cwnd = WINSIZE;
  int sendBase ;
  int nextSeq;
  int lastSeq = -1; // EOF seq num.
  // random for packet corrupt/loss.
  srand(time(NULL));
  // select
  struct timeval tv;
  fd_set sockfds;

  // Wait for SYN packet. Filemane in payload.
  while (1) {
    memset(buf, 0, BUFSIZE);
    recvlen = recvfrom(sockfd, buf, BUFSIZE, 0, (struct sockaddr *)&rec_addr,
                       &addrlen);
    // Deal with Loss and corrupt.
    if (isCorrupt()) {
      printf("%sCORRUPT: SYN\n%s", KRED, KEND);
      continue;
    } else if (isLoss()) {
      printf("%sLOSS: SYN\n%s", KRED, KEND); 
      continue;
    } else {
      if (recvHeader->flag & SYN) {
        cout << "RECEIVE: SYN, seqNum " << recvHeader->seqNum << endl;
        strcpy(filename, recvHeader->payload);
        if ((fd = fopen(filename, "rb")) == NULL) {
          cout << "File " << filename << " does not exist!" <<  endl;
        } else {
          cout << "OPEN FILE: " << filename << endl;
          // Set sendBase to seqNum.
          sendBase = recvHeader->seqNum;
          nextSeq = sendBase;
          break;
        }
      }
    }
  }

  // Sendqueue initialize.
  int ack[QSIZE];
  char sendQueue[QSIZE][BUFSIZE];
  memset(sendQueue, 0, QSIZE*BUFSIZE);
  memset(ack, 0, QSIZE);
 
  // Send SYNACK with pkt 1
  memset(buf, 0, SIZE);
  recvlen = fread(buf, 1, SIZE, fd);
  // Write header.
  memset(datagram, 0, BUFSIZE);
  sendHeader->seqNum = nextSeq;
  sendHeader->flag = (SYN | ACK); //SYNACK
  sendHeader->flag |= (feof(fd) ? END: 0);
  sendHeader->length = recvlen;
  // Write payload.
  memcpy(sendHeader->payload, buf, recvlen);

  if (sendto(sockfd, datagram, sizeof(Header), 0, (struct sockaddr *)&rec_addr,
             sizeof(rec_addr)) < 0) {
    cout << "Send msg to sender failed" << endl;
  } else {
    cout << "SEND: DATA " << nextSeq << " WITH SYNACK" << endl;
  }

  // If EOF, set lastSeq.
  if (feof(fd)) {
    lastSeq = nextSeq;
    fclose(fd);
  }

  // Wait for ACK.
  while (1) {
    tv.tv_sec = SEC;
    tv.tv_usec = USEC;
    FD_ZERO(&sockfds);
    FD_SET(sockfd, &sockfds);

    // Use select to receive ack and deal with timeout.
    select(sockfd+1, &sockfds, NULL, NULL, &tv);

    // ACK received.
    if (FD_ISSET(sockfd, &sockfds)) {
      memset(buf, 0, BUFSIZE);
      recvlen = recvfrom(sockfd, buf, BUFSIZE, 0,
                         (struct sockaddr *)&rec_addr, &addrlen);

      int ackNum = recvHeader->seqNum;       
      // Deal with Loss and corrupt.
      if (isCorrupt()) {
        printf("\t\t\t%sCORRUPT: ACK %d\n%s", KRED, ackNum, KEND);
        continue;
      } else if (isLoss()) {
        printf("\t\t\t%sLOSS: ACK %d\n%s", KRED, ackNum, KEND); 
        continue;
      } else {
        // Ignore other data except ACK. 
        if (recvHeader->flag & ACK) {
          cout << "\t\t\tRECEIVE: ACK " << ackNum << endl;
          if (sendBase == lastSeq) {
            cout << "Reliable transfer done!" << endl;

            // Send FIN data.
            memset(datagram, 0, BUFSIZE); 
            sendHeader->flag = FIN;
            sendto(sockfd, datagram, sizeof(Header), 0,
                   (struct sockaddr *)&rec_addr, sizeof(rec_addr));
            cout << "SEND: FIN (RECEIVE ALL ACK)" << endl;

            // Wait for FINACK
            while (1) {
              tv.tv_sec = SEC;
              tv.tv_usec = USEC;
              FD_ZERO(&sockfds);
              FD_SET(sockfd, &sockfds); 
              // Use select to receive ack and deal with timeout.
              select(sockfd+1, &sockfds, NULL, NULL, &tv);

              // ACK received.
              if (FD_ISSET(sockfd, &sockfds)) {
                memset(buf, 0, BUFSIZE);
                recvlen = recvfrom(sockfd, buf, BUFSIZE, 0,
                                   (struct sockaddr *)&rec_addr, &addrlen);

                if (!(recvHeader->flag & ACK)) {
                  cout << "NOT AN ACK!!" << endl;
                  continue;
                }
 
                // Deal with Loss and corrupt.
                if (isCorrupt()) {
                  printf("%sCORRUPT: FINACK\n%s", KRED, KEND);
                  continue;
                } else if (isLoss()) {
                  printf("%sLOSS: FINACK\n%s", KRED, KEND); 
                  continue;
                } else {
                  cout << "RECEIVE: FINACK, terminate." << endl;
                  return 0;
                }
              } else { //timeout
                sendto(sockfd, datagram, sizeof(Header), 0,
                       (struct sockaddr *)&rec_addr, sizeof(rec_addr));
                cout << "RESEND: FIN (RECEIVE ALL ACK)" << endl;
              }
            }
          }
          // Increment sendBase and nextSeq.
          sendBase = (sendBase == QSIZE-1)? 0: sendBase + 1;
          nextSeq = sendBase;
          break;
        }
      }  
    } else { // Timeout, resend data.
      sendto(sockfd, datagram, sizeof(Header), 0, (struct sockaddr *)&rec_addr,
             sizeof(rec_addr));
      cout << "RESEND: DATA " << nextSeq << " WITH SYNACK" << endl;
    }
  }

  // Send remaining file.
  while (1) {
    // Unused slots exist in window.
    while ((((nextSeq >= sendBase) && (nextSeq - sendBase < cwnd)) ||
           ((nextSeq < sendBase) && ((QSIZE - sendBase) + nextSeq < cwnd)))
           && lastSeq == -1 ) { 

      // Read file.
      memset(buf, 0, SIZE);
      recvlen = fread(buf, 1, SIZE, fd);
      // Construct data. 
      memset(datagram, 0, BUFSIZE); 
      // Write header.
      sendHeader->seqNum = nextSeq;
      sendHeader->flag |= (feof(fd)? END: 0);
      sendHeader->length = recvlen;
      // Write payload.
      memcpy(sendHeader->payload, buf, recvlen);

      // Write data to queue, set ACK to 0.
      memcpy(sendQueue[nextSeq], datagram, BUFSIZE);
      ack[nextSeq] = 0;

      // Send data.
      if (sendto(sockfd, sendQueue[nextSeq], sizeof(Header), 0,
          (struct sockaddr *)&rec_addr, sizeof(rec_addr)) < 0) {
        cout << "Send msg to sender failed" << endl;
      } else {
        cout << "SEND: DATA " << nextSeq << endl;
      }

      // Set lestSeq if EOF.
      if (feof(fd)) {
        lastSeq = nextSeq;
        fclose(fd);
      } 
 
      // Increment sequence number. (circular)
      nextSeq = (nextSeq == QSIZE-1) ? 0: nextSeq + 1; 
    }

    // Wait for ACK.
    while (1) { 
      tv.tv_sec = SEC;
      tv.tv_usec = USEC;
      FD_ZERO(&sockfds);
      FD_SET(sockfd, &sockfds);
      // Use select to receive ack and deal with timeout.
      select(sockfd+1, &sockfds, NULL, NULL, &tv);

      // ACK received.
      if (FD_ISSET(sockfd, &sockfds)) {
        memset(buf, 0, BUFSIZE);
        recvlen = recvfrom(sockfd, buf, BUFSIZE, 0,
                           (struct sockaddr *)&rec_addr, &addrlen);

        if (!(recvHeader->flag & ACK)) {
          cout << "NOT AN ACK!!" << endl;
          continue;
        }
        int ackNum = recvHeader->seqNum;
        
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
        
        // If ackNum == sendBase, update sendBase.
        if (ackNum == sendBase) {
          while (ack[sendBase]) {
            ack[sendBase] = 0; // clear ACK queue.
            // If ACK to the last seq, file has been reliably received.
            if (sendBase == lastSeq) {
              cout << "Reliable transfer done!" << endl;

              // Send FIN data.
              memset(datagram, 0, BUFSIZE); 
              sendHeader->flag = FIN;
              sendto(sockfd, datagram, sizeof(Header), 0,
                     (struct sockaddr *)&rec_addr, sizeof(rec_addr));
              cout << "SEND: FIN (RECEIVE ALL ACK)" << endl;

              // Wait for FINACK
              while (1) {
                tv.tv_sec = SEC;
                tv.tv_usec = USEC;
                FD_ZERO(&sockfds);
                FD_SET(sockfd, &sockfds);
                // Use select to receive ack and deal with timeout.
                select(sockfd+1, &sockfds, NULL, NULL, &tv);

                // ACK received.
                if (FD_ISSET(sockfd, &sockfds)) {
                  memset(buf, 0, BUFSIZE);
                  recvlen = recvfrom(sockfd, buf, BUFSIZE, 0,
                                     (struct sockaddr *)&rec_addr, &addrlen);

                  if (!(recvHeader->flag & ACK)) {
                    cout << "NOT AN ACK!!" << endl;
                    continue;
                  }
 
                  // Deal with Loss and corrupt.
                  if (isCorrupt()) {
                    printf("%sCORRUPT: FINACK\n%s", KRED, KEND);
                    continue;
                  } else if (isLoss()) {
                    printf("%sLOSS: FINACK\n%s", KRED, KEND); 
                    continue;
                  } else {
                    cout << "RECEIVE: FINACK, terminate." << endl;
                    return 0;
                  }
                } else { //timeout
                  sendto(sockfd, datagram, sizeof(Header), 0,
                         (struct sockaddr *)&rec_addr, sizeof(rec_addr));
                  cout << "RESEND: FIN (RECEIVE ALL ACK)" << endl;
                }
              }
            }
            // Increment sendBase.
            sendBase = (sendBase == QSIZE-1)? 0: sendBase + 1;
          }
          // If sendBase is incremented, break while to send more data.
          break;
        }
        // If ackNum != sendBase, continue to listen for ACK.
      } else { // Timeout. Resend sendBase.
        // Resend pkt from queue.
        if (sendto(sockfd, sendQueue[sendBase], sizeof(Header), 0,
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
