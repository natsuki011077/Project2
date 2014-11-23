#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <signal.h>
#include <iostream>
#include <cstring>
#include <netdb.h>


#define BUFSIZE 1024

void error(char *msg)
{
    perror(msg);
    exit(1);
}

int main (int argc, char* argv[])
{
  struct sockaddr_in sender_addr; // sender address
  struct sockaddr_in rec_addr; // receiver address
  socklen_t addrlen = sizeof(rec_addr);

  int sockfd; // server socket

  unsigned char buf[BUFSIZE]; // receive buffer
  int recvlen; // # bytes received

  // Make sure the port number is provided.   
  if (argc < 2) {
    cout << "USAGE: " << argv[0] << " PORT" << endl;
    exit(1);
  }

  // Obtain the fd for UDP listening socket.
  sockfd = socket(AF_INET, SOCK_DGRAM, 0);
  if (sockfd < 0) {
    error("ERROR: sender open socket failure\n");
  }
  cout << "Sender socket created" << endl;


  int port = atoi(argv[1]);
  
  memset((char*) &sender_addr, 0, sizeof(sender_addr)) ;
  sender_addr.sin_family = AF_INET;
  sender_addr.sin_addr.s_addr = INADDR_ANY;
  sender_addr.sin_port = htons(port);

  if (bind(sockfd, (struct sockaddr *) &sender_addr, sizeof(sender_addr)) < 0) {
    close(sockfd);
    error("ERROR: sender bind socket failure\n");
  }
  cout << "server bind to port " << sockfd << ", done" << endl;

  while (1) {
    recvlen = recvfrom(sockfd, buf, BUFSIZE, 0, (struct sockaddr *)&rec_addr, &addrlen);
    cout << "receive msg: " << buf << endl;
  }
  
  return 0;
}
