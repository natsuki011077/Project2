#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <signal.h>

#include <iostream>
#include <string>
#include <cstring>

using namespace std;

void error(char *msg)
{
    perror(msg);
    exit(1);
}

int main(int argc, char* argv[])
{
  int sockfd; //Socket descriptor
  int portno;
  struct sockaddr_in serv_addr;
  struct hostent *server; //contains tons of information, including the server's IP address

  char buf[BUFSIZE];

  if (argc < 4) {
    fprintf(stderr,"usage %s hostname port filename\n", argv[0]);
    exit(0);
  }

  portno = atoi(argv[2]);
  string filename(argv[3]);

  sockfd = socket(AF_INET, SOCK_DGRAM, 0);
  if (sockfd < 0)
    error("ERROR opening socket");

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
  msg.append("retrive ");
  msg += filename;

  if (sendto(sockfd, msg.c_str(), msg.length() + 1, 0, (struct sockaddr *)&servaddr, sizeof(servaddr)) < 0) {
    error("Send msg to sender failed\n");
    return 0;
  }
  
}
