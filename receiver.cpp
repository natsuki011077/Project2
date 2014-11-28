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
  char msg[4];
  struct ACKHeader *ack = (struct ACKHeader *) msg;
  ack->ackNum = seq;
  sendto(sockfd, msg, sizeof(msg), 0, (struct sockaddr *)&serv_addr, sizeof(serv_addr));
  cout << "  SEND: ACK " << seq << endl;
}

int main(int argc, char* argv[])
{
  int sockfd; //Socket descriptor
  int portno;
  struct sockaddr_in serv_addr;
  struct hostent *server; //contains tons of information, including the server's IP address
  char filename[200];
 
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

  // Send retrieve message.
  char msg[256];
  strcpy(msg, "RETRIEVE: ");
  strcat(msg, filename); 
  if (sendto(sockfd, msg, strlen(msg), 0, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
    printf("Send msg to sender failed\n");
    return 0;
  }
  cout << "SEND MSG: " << msg << endl;

  int received[QSIZE];
  char recvQueue[QSIZE][BUFSIZE];
  int winSize = WINSIZE;
  int recBase = 0;

  struct sockaddr_in rec_addr; // receiver address
  socklen_t addrlen = sizeof(rec_addr);
  int recvlen; // # bytes received
  char buf[BUFSIZE]; 

  uint32_t seqNum;
  unsigned int recvWin = WINSIZE;
  int recvBase = 0;
  int recvFront = QSIZE - recvWin;

  // Outputfile
  FILE *fd;
  char outFilename[256];
  strcpy(outFilename, "rec_");
  strcat(outFilename, filename);
  fd = fopen(outFilename, "wb");

  srand(time(NULL));  
  // Receive File
  while (1) {
    memset(buf, 0, BUFSIZE);
    recvlen = recvfrom(sockfd, buf, sizeof(SendHeader), 0,
                       (struct sockaddr *)&rec_addr, &addrlen);  

    // Parse header.
    struct SendHeader *header = (struct SendHeader *) buf;
    seqNum = header->seqNum;

    // Check if data is corrupt or loss.
    if (isCorrupt()) {
      cout << "CORRUPT: DATA " << seqNum << endl;
      continue;  
    } else if (isLoss()) {
      cout << "LOSS: DATA " << seqNum << endl;
      continue;
    } else {
      cout << "RECEIVE: DATA " << seqNum << endl;
    }
    
    // Seq is in [rcv_base, rcv_base+N-1]
    if(((seqNum >= recvBase) && (seqNum - recvBase < recvWin)) ||
       ((seqNum < recvBase) && (seqNum < (recvWin - (QSIZE - recvBase))))) {
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

          header = (struct SendHeader *) recvQueue[recvBase];
          uint16_t EOFFlag = header->EOFFlag;
          uint16_t length = header->length;

          // Write data to file.
          fwrite(header->payload, 1, length, fd);
          cout << "    WRITE: DATA " << recvBase << " to file." << endl;
          
          // Received all data. 
          if (EOFFlag) {
            fclose(fd);
            cout << "File transfer complete. Wait for sender complete." << endl;
            while (1) {
              recvlen = recvfrom(sockfd, buf, sizeof(SendHeader), 0,
                                 (struct sockaddr *)&rec_addr, &addrlen);
              // Exit if sender sends terminate message.
              if (strncmp(buf, "TRANSFER DONE", 13) == 0) {
                cout << "Sender received all ACK, close connection." << endl;
                return 0;
              }
              // Data received.
              struct SendHeader *header = (struct SendHeader *) buf;
              if (isCorrupt()) {
                cout << "CORRUPT: DATA " << header->seqNum << endl;
                continue;
              } else if (isLoss()) {
                cout << "LOSS: DATA " << header->seqNum << endl;
                continue;
              } else {
                cout << "RECEIVE: DATA " << header->seqNum << endl;
                SendACK(sockfd, serv_addr, header->seqNum); // Send ACK
              }
            }
          }

          received[recvBase] = 0;
          // Update base.
          recvBase = (recvBase == QSIZE - 1)? 0: recvBase + 1;
          recvFront = (recvFront == QSIZE)? 0: recvFront + 1;          
        }
      }
    } else if (((seqNum >= recvFront) && (seqNum - recvFront < recvWin)) ||
               ((seqNum < recvFront) && 
                (seqNum < (recvWin - (QSIZE - recvFront))))) {
      // Seq is in [rcv_base-N, rcv_base-1]. Send ACK.   
      SendACK(sockfd, serv_addr, seqNum);
    } else {
      // Ignore.
    }
  }
  return 0;
}
