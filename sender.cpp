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

#define BUFSIZE 1024
#define WINSIZE 32
#define QUEUESIZE 1024
#define DEBUG 1

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
   
  while (1) {
    // Wait for retrieve msg
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

  // Send file.
  int winSize = WINSIZE;
  char buffer[BUFSIZE-2];

  // sendqueue
  vector<int> ack(QUEUESIZE);
  vector<string> sendQueue(QUEUESIZE);
  int sendBase = 0;
  short int nextSeq = 1;
  

  while (!infile.eof()) {
    infile.read(buffer, BUFSIZE - 2);
    int length = infile.gcount();
 
    char msg[BUFSIZE];
    // Write seq num.
    strcpy(msg,"");
    msg[0] = (nextSeq) & 0xFF;
    msg[1] = (nextSeq >> 8) & 0xFF;
//    msg[2] = (nextSeq >> 16) & 0xFF;
//    msg[3] = (nextSeq >> 24) & 0xFF;

    // Write payload.
    strcat(msg, buffer);

    // Write queue
    sendQueue[nextSeq].assign(buffer);
    ack[nextSeq] = 0;

#ifdef DEBUG
    cout << "sendQueue = " << sendQueue[nextSeq] << endl;
    cout << "msg = " << msg << endl;
#endif

    // Send msg.
    if (sendto(sockfd, msg, strlen(msg), 0, (struct sockaddr *)&rec_addr, sizeof(rec_addr)) < 0) {
      printf("Send msg to sender failed\n");
    } else {
      cout << "pkt " << nextSeq << " send!" << endl;
      nextSeq = (nextSeq == QUEUESIZE) ? 0: nextSeq + 1;
    }
  } 

  return 0;
}
