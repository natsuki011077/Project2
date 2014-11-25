#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/wait.h>
#include <signal.h>

#include <cstdlib>
#include <cstring>
#include <iostream>
#include <fstream>
#include <stdio.h>
#include <vector>


#include <sys/time.h>
#define BUFSIZE 1024
#define WINSIZE 32
#define QUEUESIZE 1024
#define DEBUG 1

typedef struct Response {
  int32_t seqNum;
  int32_t EOFFlag;
  char data[BUFSIZE - 24];
} Response;


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
//    close(sockfd);
    printf("ERROR: sender bind socket failure\n");
  }
  printf("server bind to port %d, done\n", sockfd);

  ifstream infile;
  // Wait for retrieve message. 
  while (1) {
    recvlen = recvfrom(sockfd, buf, BUFSIZE, 0, (struct sockaddr *)&rec_addr, &addrlen);
    cout << "receive msg: " << buf << endl;
 
    string buffer(buf);
    if (buffer.compare(0, 10, "retrieve: ") == 0) {
      // Check if file exists.
      string filename = buffer.substr(10);
      infile.open(filename.c_str());
      if (!infile) {
        cout << "filename: " << filename << " does not exist!" << endl;
      } else {
        cout << "open filename: " << filename << endl;
        break;
      }
    }
  }


  int winSize = WINSIZE;
  char buffer[BUFSIZE-24];
  // sendqueue
  vector<int> ack(QUEUESIZE);
  vector<string> sendQueue(QUEUESIZE);
  int sendBase = 0;
  int nextSeq = 0;
  // select
  struct timeval tv;
  fd_set sockfds;
  // timeout 2.5s
  tv.tv_sec = 2;
  tv.tv_usec = 500000;  
  FD_ZERO(&sockfds);
  FD_SET(sockfd, &sockfds);

  // Send file.
  int lastSeq = -1; // EOF seq num.

  while (1) {
    while ((nextSeq < sendBase + winSize) && lastSeq == -1) {
      // More slots in window.
      // Read file.
      infile.read(buffer, BUFSIZE - 24);
      int length = infile.gcount();
 
      Respose temp;
      temp.seq = seqNum;
      temp.EOFFlag = (infile.eof()) ? 1: 0;
      if (infile.eof()) {
        last
/*
      // Write message.
      char msg[BUFSIZE];
      char bytes1[4];
      char bytes2[4];
      // Write seq num.
      strcpy(msg,"");
      bytes1[0] = (nextSeq) & 0xFF;
      bytes1[1] = (nextSeq >> 8) & 0xFF;
      bytes1[2] = (nextSeq >> 16) & 0xFF;
      bytes1[3] = (nextSeq >> 24) & 0xFF;
      strcat(msg, bytes1);
      // Write EOF bit.
      if (infile.eof()) {
        bytes2[0] = 1 & 0xFF;
        lastSeq = nextSeq;
        infile.close();
      } else {
        bytes2[0] = 0 & 0xFF;
      }
      bytes2[1] = 0 & 0xFF;
      bytes2[2] = 0 & 0xFF;
      bytes2[3] = 0 & 0xFF;
      strcat(msg, bytes2);
      cout << "msg = " << msg << endl;
      int b;
      scanf("%d", &b);
 
      // Write payload.
      strcat(msg, buffer);

      // Write queue.
      sendQueue[nextSeq].assign(msg);
      ack[nextSeq] = 0;
 
#ifdef DEBUG
      cout << "sendQueue = " << sendQueue[nextSeq] << endl;
      cout << "msg = " << msg << endl;
#endif
      scanf("%d", &b);

      // Send msg.
      if (sendto(sockfd, msg, strlen(msg), 0, (struct sockaddr *)&rec_addr, sizeof(rec_addr)) < 0) {
        printf("Send msg to sender failed\n");
      } else {
        cout << "Send: pkt " << nextSeq << endl;
      }
*/ 

     // increment sequence number
      nextSeq = nextSeq + 1; 

   }

    // Wait for ACK.
    int update = 0;
    while (!update) {
      // Use select for timeout.
      select(sockfd+1, &sockfds, NULL, NULL, &tv);

      if (FD_ISSET(sockfd, &sockfds)) { // Receive ACK.
        recvlen = recvfrom(sockfd, buf, BUFSIZE, 0, (struct sockaddr *)&rec_addr, &addrlen);
        int ackNum = buf[0] || (buf[1] << 8) || (buf[2] << 16) || (buf[3] << 24);
        ack[ackNum] = 1;
        cout << "Receive ACK " << ackNum << endl;
        int c;
        scanf("%d", &c); 
        // If ackNum != sendBase, continue to listen for ACK.
 
        // If ackNum == sendBase, update sendBase
        if (ackNum == sendBase) {
          while (ack[sendBase]) {
            ack[sendBase] = 0; // clear ACK queue.
            sendBase = (sendBase == QUEUESIZE) ? 0: sendBase + 1;
          }
          // If ACK to the last seq, file has been reliably received.
          if (sendBase == lastSeq - 1) {
            cout << "Transfer done!" << endl;
            return 0;
          } else {
            // Set update to 1 to send more pkt.
            update = 1;
          }
        }
      } else { // Timeout. Resend base.
        // Write message.
        char msg1[BUFSIZE];
        strcat(msg1, sendQueue[sendBase].c_str());

        // Resend msg.
        if (sendto(sockfd, msg1, strlen(msg1), 0, (struct sockaddr *)&rec_addr, sizeof(rec_addr)) < 0) {
          printf("Send msg to sender failed\n");
        } else {
          cout << "Resend: pkt " << sendBase << endl;
        }
      }    
    }

  }
  return 0;
}
