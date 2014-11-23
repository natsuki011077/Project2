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

#define BUFSIZE 1024
#define QUEUESIZE 65535
#define WINSIZE 32

using namespace std;


int GetAckNum(string &buf)
{
  char str[33];
  size_t len = buf.copy(str, 32, 32);
  str[len] = '\0';
  return atoi(str);
}

int GetSeqNum(string &buf)
{
  return buf[0];
}

int main(int argc, char* argv[])
{
  int sockfd; //Socket descriptor
  int portno;
  struct sockaddr_in serv_addr;
  struct hostent *server; //contains tons of information, including the server's IP address

  if (argc < 4) {
    fprintf(stderr,"usage %s hostname port filename\n", argv[0]);
    exit(0);
  }

  portno = atoi(argv[2]);
  string filename(argv[3]);

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

  string msg;
  msg.append("retrieve: ");
  msg += filename;
  if (sendto(sockfd, msg.c_str(), strlen(msg.c_str()), 0, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
    printf("Send msg to sender failed\n");
    return 0;
  }

  vector<int> ack(QUEUESIZE,0);
  vector<string> recvQueue(QUEUESIZE);
  int winSize = WINSIZE;
  int recBase = 0;

  struct sockaddr_in rec_addr; // receiver address
  socklen_t addrlen = sizeof(rec_addr);
  int recvlen; // # bytes received
  char buf[BUFSIZE]; 

  int ackNum;
  int seqNum;
  // Receive File
  while (1) {
    recvlen = recvfrom(sockfd, buf, BUFSIZE, 0, (struct sockaddr *)&rec_addr, &addrlen);  
    
    string buffer(buf, recvlen);
    cout << "receive msg = " << buffer << endl;
//    ackNum = GetAckNum(buffer);
    seqNum = GetSeqNum(buffer);
    cout << "Seq: " << seqNum << endl;
    string payload = buffer.substr(1);
    cout << "Payload: " << payload << endl;
//    cout << "Ack: " << ackNum << "  Seq: " << seqNum << endl;
  }
  
  return 0;
}
