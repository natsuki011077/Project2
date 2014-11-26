#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>

#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <signal.h>
#include <netdb.h>

#include <string>
#include <vector>
#include <iostream>

#include "header.h"

using namespace std;

void SendACK(int sockfd, struct sockaddr_in& serv_addr, int seq)
{
  char msg[4];
  struct ACKHeader *ack = (struct ACKHeader *) msg;
  ack->ackNum = seq;
  sendto(sockfd, msg, sizeof(msg), 0, (struct sockaddr *)&serv_addr, sizeof(serv_addr));
  cout << "SEND: ACK " << seq << endl;
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
  strcpy(msg, "retrieve: ");
  strcat(msg, filename); 
  if (sendto(sockfd, msg, strlen(msg), 0, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
    printf("Send msg to sender failed\n");
    return 0;
  }
  cout << "SEND: " << msg << endl;

  int received[QUEUESIZE];
  char recvQueue[QUEUESIZE][BUFSIZE];
  int winSize = WINSIZE;
  int recBase = 0;

  struct sockaddr_in rec_addr; // receiver address
  socklen_t addrlen = sizeof(rec_addr);
  int recvlen; // # bytes received
  char buf[BUFSIZE]; 

  uint32_t seqNum;
  unsigned int recvWin = WINSIZE;
  int recvBase = 0;
  int recvEnd = recvBase + recvWin - 1;
  int recvFront = -recvWin;

  // Outputfile
  FILE *fd;
  char outFilename[256];
  strcpy(outFilename, "rec_");
  strcat(outFilename, filename);
  fd = fopen(outFilename, "wb");
  
//  int discard = 0;
  // Receive File
  while (1) {

    memset(buf, 0, BUFSIZE);
    recvlen = recvfrom(sockfd, buf, sizeof(SendHeader), 0, (struct sockaddr *)&rec_addr, &addrlen);  

    // Parse header.
    struct SendHeader *header = (struct SendHeader *) buf;
    seqNum = header->seqNum;
    cout << "RECEIVE: PKT: " << seqNum << endl;
/*
    if (!discard && seqNum == 8) {
      cout << "discard pkt 8" << endl;
      discard = 1;
      continue;
    }
*/
    // Seq is in [rcv_base, rcv_base+N-1]
    if ((seqNum >= recvBase) && (seqNum <= recvEnd)) {
      // Write queue buffer
      received[seqNum] = 1;
      memcpy(recvQueue[seqNum], buf, BUFSIZE);
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
          cout << "WRITE: PKT " << recvBase << " to file." << endl;
          
          // Received all data, exit. 
          if (EOFFlag) {
            fclose(fd);
            cout << "File transfer complete." << endl;
            return 0;
          }

          received[recvBase] = 0;
          // Update base.
          recvBase = recvBase + 1;
          recvEnd = recvEnd + 1;
          recvFront = recvFront + 1;
        }
      }
    } else if ((seqNum < recvBase) && (seqNum >= recvFront)) { // Seq is in [rcv_base-N, rcv_base-1]
      // Send ACK.
      SendACK(sockfd, serv_addr, seqNum);
    } else {
      // Ignore.
    }
  }
  
  return 0;
}
