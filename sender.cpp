#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <signal.h>

void error(char *msg)
{
    perror(msg);
    exit(1);
}

int main (int argc, char* argv[])
{
  // Make sure the port number is provided.   
  if (argc < 2) {
    fprintf(stderr,"USAGE: %s PORT\n", argv[0]);
    exit(1);
  }

  // Obtain the fd for UDP listening socket.
  sockfd = socket(AF_INET, SOCK_DGRAM, 0);
  if (sockfd < 0) {
    error("ERROR: sender open socket failure\n");
  }
  printf("Sender socket created\n");


  int port = atoi(argv[1]);
  struct sockaddr_in sender_addr;
  
  memset((char*) &sender_addr, 0, sizeof(sender_addr)) ;
  sender_addr.sin_family = AF_INET;
  sender_addr.sin_addr.s_addr = INADDR_ANY;
  sender_addr.sin_port = htons(port);

  if (bind(sockfd, (struct sockaddr *) &sender_addr, sizeof(sender_addr)) < 0) {
    close(sockfd);
    error("ERROR: sender bind socket failure\n");
  }
  printf("server bind to port %d, done\n", sockfd);

  // Listen to receiver
  while (1) {
    
  }
  

  return 0;
}
