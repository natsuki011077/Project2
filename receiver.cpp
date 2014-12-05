#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>

#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <signal.h>
#include <netdb.h>

#include <iostream>

#include "header.h"
using namespace std;

void SendACK(int sockfd, struct sockaddr_in& serv_addr, int seq)
{
  char datagram[BUFSIZE];
  memset(datagram, 0, BUFSIZE);
  struct Header *header = (struct Header *) datagram;
  header->seqNum = seq;
  header->flag |= ACK;
  sendto(sockfd, datagram, sizeof(Header), 0, (struct sockaddr *)&serv_addr,
         sizeof(serv_addr));
  timestamp();
  cout << "SEND: ACK " << seq << endl;
}

int main(int argc, char* argv[])
{
  int sockfd; //Socket descriptor
  int portno;
  struct sockaddr_in serv_addr;
  struct hostent *server; //contains tons of information, including the server's IP address
  char filename[200];

  struct sockaddr_in rec_addr; // receiver address
  socklen_t addrlen = sizeof(rec_addr);

  int recvlen; // # bytes received
  char buf[BUFSIZE]; // receive buffer
  char datagram[BUFSIZE]; //sender buffer
  // header parser pointer for receive data
  struct Header *recvHeader = (struct Header *) buf;
  // header parser pointer for send data
  struct Header *sendHeader = (struct Header *) datagram;
  // header parser pointer for queue data
  struct Header *qHeader;

  if (argc < 4) {
    fprintf(stderr,"usage %s hostname port filename\n", argv[0]);
    exit(0);
  }

  portno = atoi(argv[2]);
  strcpy(filename, argv[3]);

  sockfd = socket(AF_INET, SOCK_DGRAM, 0);
  if (sockfd < 0)
    printf("ERROR opening socket");

  server = gethostbyname(argv[1]);
  if (server == NULL) {
    fprintf(stderr,"ERROR, no such host\n");
    exit(0);
  }

  bzero((char *) &serv_addr, sizeof(serv_addr));
  serv_addr.sin_family = AF_INET; //initialize server's address
  bcopy((char *)server->h_addr, (char *)&serv_addr.sin_addr.s_addr, server->h_length);
  serv_addr.sin_port = htons(portno);

  // random
  srand(time(NULL)); 
  int recvWin = WINSIZE;
  int recvBase = rand()%QSIZE; // Random select base.
  int recvFront = (recvBase >= recvWin) ? recvBase - recvWin:
                                          (QSIZE - recvWin) + recvBase;
  int32_t seqNum;

  // select
  struct timeval tv;
  fd_set sockfds;

  // Reciever's queue.
  int received[QSIZE];
  memset(received, 0, QSIZE*sizeof(int));
  char recvQueue[QSIZE][BUFSIZE];

  // Outputfile.
  FILE *fd;
  char outFilename[256];
  strcpy(outFilename, "rec_");
  strcat(outFilename, filename);
  fd = fopen(outFilename, "wb");

  // Construct SYN data. payload = filename.
  memset(datagram, 0, BUFSIZE);
  // Write header.
  sendHeader->seqNum = recvBase;
  sendHeader->flag |= SYN;
  sendHeader->length = strlen(filename);
  // Write payload.
  memcpy(sendHeader->payload, filename, strlen(filename));
  if (sendto(sockfd, datagram, sizeof(Header), 0, (struct sockaddr *)&serv_addr,
             sizeof(serv_addr)) < 0) {
    cout << "Send msg to sender failed" << endl;
    return 0;
  }
  timestamp();
  cout << "SEND SYN MESSAGE FOR FOR " << filename;
  cout << ", SET SENDBASE: " << sendHeader->seqNum << endl;

  // Wait for SYNACK.
  while (1) {
    tv.tv_sec = SEC;
    tv.tv_usec = USEC;
    FD_ZERO(&sockfds);
    FD_SET(sockfd, &sockfds);
    // Wait for SYNACK. If timeout, resend request msg.
    select(sockfd+1, &sockfds, NULL, NULL, &tv);

    if (FD_ISSET(sockfd, &sockfds)) {
      memset(buf, 0, BUFSIZE);
      recvlen = recvfrom(sockfd, buf, sizeof(Header), 0,
                         (struct sockaddr *)&rec_addr, &addrlen);  

      // Check if data is corrupt or loss.
      timestamp();
      if (isCorrupt()) {
        printf("%sCORRUPT: SYNACK\n%s", KRED, KEND);
        continue;
      } else if (isLoss()) {
        printf("%sLOSS: SYNACK\n%s", KRED, KEND);
        continue;
      } else {
        // Parse header.
        if (recvHeader->flag & (SYN | ACK) || 
            recvHeader->flag & (SYN | ACK | END)) {
          cout << "RECEIVE: SYNACK"  << endl;
          // Send ACK.
          SendACK(sockfd, serv_addr, recvHeader->seqNum);
          // Write data to file.
          fwrite(recvHeader->payload, 1, recvHeader->length, fd);
          cout << "\t\t\t\t\t\tWRITE: DATA " << recvBase << " to file." << endl;
          // If no more data to be transferd.
          if (recvHeader->flag & END) {
            fclose(fd);
            cout << "File transfer complete. Wait for sender complete." << endl;
            while (1) {
              recvlen = recvfrom(sockfd, buf, sizeof(Header), 0,
                                 (struct sockaddr *)&rec_addr, &addrlen);
              // Data received.
              timestamp();
              if (recvHeader->flag & FIN) {
                timestamp();
                if (isCorrupt()) {
                  printf("%sCORRUPT: FIN\n%s", KRED, KEND);
                  continue;
                } else if (isLoss()) {
                  printf("%sLOSS: FIN\n%s", KRED, KEND);
                  continue;
                } else {
                  cout << "RECEIVE FIN." << endl;
                  // Send FINACK
                  memset(datagram, 0, BUFSIZE);
                  // Write header.
                  sendHeader->flag |= (FIN | ACK);
                  sendto(sockfd, datagram, sizeof(Header), 0,
                         (struct sockaddr *)&serv_addr, sizeof(serv_addr)); 
                  timestamp();
                  cout << "SEND FINACK" << endl;
                  while (1) {
                    // Set timer to 10 sec.
                    tv.tv_sec = 10;
                    tv.tv_usec = 0;
                    FD_ZERO(&sockfds);
                    FD_SET(sockfd, &sockfds);

                    select(sockfd+1, &sockfds, NULL, NULL, &tv);
                    if (FD_ISSET(sockfd, &sockfds)) {
                      memset(buf, 0, BUFSIZE);
                      recvlen = recvfrom(sockfd, buf, sizeof(Header), 0,
                                         (struct sockaddr *)&rec_addr, &addrlen);  
                      // Receive FIN, send FINACK. (FINACK may be lost).
                      if (recvHeader->flag & (FIN)) {
                        sendto(sockfd, datagram, sizeof(Header), 0,
                               (struct sockaddr *)&serv_addr, sizeof(serv_addr));
                        timestamp(); 
                        cout << "SEND FINACK" << endl;
                      }
                    } else {
                      // Timeout, terminate.
                      return 0;
                    }
                  }
                }
              } else if (isCorrupt()) {
                printf("\t\t\t%sCORRUPT: DATA %d\n%s", KRED, recvHeader->seqNum, KEND);
                continue;
              } else if (isLoss()) {
                printf("\t\t\t%sLOSS: DATA %d\n%s", KRED, recvHeader->seqNum, KEND);
                continue;
              } else {
                cout << "\t\t\tRECEIVE: DATA " << recvHeader->seqNum << endl;
                SendACK(sockfd, serv_addr, recvHeader->seqNum); // Send ACK
              }
            }
          }
          // Increment recvBase.
          recvBase = (recvBase == QSIZE - 1)? 0: recvBase + 1;
          recvFront = (recvFront == QSIZE - 1)? 0: recvFront + 1;
          break;
        }
      }
    } else { // timeout. Resend request.
      memset(datagram, 0, BUFSIZE);
      // Write header.
      sendHeader->seqNum = recvBase;
      sendHeader->flag |= SYN;
      sendHeader->length = strlen(filename);
      // Write payload.
      memcpy(sendHeader->payload, filename, strlen(filename));
      if (sendto(sockfd, datagram, sizeof(Header), 0, (struct sockaddr *)&serv_addr,
                 sizeof(serv_addr)) < 0) {
        cout << "Send msg to sender failed" << endl;
        return 0;
      }
      timestamp();
      cout << "RESEND SYN MESSAGE FOR FOR " << filename;
      cout << ", SET SENDBASE: " << sendHeader->seqNum << endl;
    }
  }

  // Receive remain file.
  while (1) {
    memset(buf, 0, BUFSIZE);
    recvlen = recvfrom(sockfd, buf, sizeof(Header), 0,
                       (struct sockaddr *)&rec_addr, &addrlen);  

    timestamp();
    if (recvHeader->flag & FIN) {
      if (isCorrupt()) {
        printf("%sCORRUPT: FIN\n%s", KRED, KEND);
        continue;
      } else if (isLoss()) {
        printf("%sLOSS: FIN\n%s", KRED, KEND);
        continue;
      } else {
        // Receive FIN.
        cout << "RECEIVE FIN" << endl;
        // Send FINACK.
        memset(datagram, 0, BUFSIZE);
        sendHeader->flag |= (FIN | ACK);
        sendto(sockfd, datagram, sizeof(Header), 0,
               (struct sockaddr *)&serv_addr, sizeof(serv_addr)); 
        timestamp();
        cout << "SEND FINACK" << endl;
        // Wait for 10 sec. and terminate.
        while (1) {
          tv.tv_sec = 10;
          tv.tv_usec = 0;
          FD_ZERO(&sockfds);
          FD_SET(sockfd, &sockfds);

          select(sockfd+1, &sockfds, NULL, NULL, &tv);

          if (FD_ISSET(sockfd, &sockfds)) {
            memset(buf, 0, BUFSIZE);
            recvlen = recvfrom(sockfd, buf, sizeof(Header), 0,
                               (struct sockaddr *)&rec_addr, &addrlen);  
            // Receive FIN, send FINACK. (FINACK may be lost).
            if (recvHeader->flag & FIN) {
              sendto(sockfd, datagram, sizeof(Header), 0,
                     (struct sockaddr *)&serv_addr, sizeof(serv_addr));
              timestamp();
              cout << "SEND FINACK" << endl;
            }
          } else {
            // Timeout, terminate.
            return 0;
          }
        }
      }
    } else {
      seqNum = recvHeader->seqNum;
      if (isCorrupt()) {
        printf("\t\t\t%sCORRUPT: DATA %d\n%s", KRED, seqNum, KEND);
        continue;  
      } else if (isLoss()) {
        printf("\t\t\t%sLOSS: DATA %d\n%s", KRED, seqNum, KEND);
        continue;
      } else {
        cout << "\t\t\tRECEIVE: DATA " << seqNum << endl;
      }
  
      // Seq is in [rcv_base, rcv_base+N-1]
      if(((seqNum >= recvBase) && (seqNum - recvBase < recvWin)) ||
         ((seqNum < recvBase) && (QSIZE - recvBase) + seqNum < recvWin)) {
//        cout << "Seq is in [rcv_base, rcv_base+N-1]" <<  endl;
        // Write queue buffer if it's not a duplicate data.
        if (!received[seqNum]) {
          memcpy(recvQueue[seqNum], buf, BUFSIZE);
          received[seqNum] = 1;
        }
        // Send ACK.
        SendACK(sockfd, serv_addr, seqNum);
        // Write payload to file if recvBase is received.
       if (seqNum == recvBase) {
          while (received[recvBase]) {           
            // Write data to file.
            qHeader = (struct Header *) recvQueue[recvBase];
            fwrite(qHeader->payload, 1, qHeader->length, fd);
            cout << "\t\t\t\t\t\tWRITE: DATA " << recvBase << " to file." << endl;
            // Received all data. 
            if (qHeader->flag & END) {
              fclose(fd);
              timestamp();
              cout << "File transfer complete. Wait for sender complete." << endl;
            }
            // Clear recvQueue after data is written to output file.
            received[recvBase] = 0;
            // Update base.
            recvBase = (recvBase == QSIZE - 1)? 0: recvBase + 1;
            recvFront = (recvFront == QSIZE - 1)? 0: recvFront + 1;          
          }
        }
      } else if (((seqNum >= recvFront) && (seqNum - recvFront < recvWin)) ||
                 ((seqNum < recvFront) && 
                  ((QSIZE - recvFront) + seqNum < recvWin))) {
        // Seq is in [rcv_base-N, rcv_base-1]. Send ACK.
//        cout << "Seq is in [rcv_base-N, rcv_base-1]. Send ACK" << endl;
        SendACK(sockfd, serv_addr, seqNum);
      } else {
        // Ignore.
      }
    }
  }
  return 0;
}
