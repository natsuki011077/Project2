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
  return 0;
}
