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
#include <fstream>

#define BUFSIZE 1024
#define QUEUESIZE 65535
#define WINSIZE 32

using namespace std;

int GetSeqNum(string &buf)
{
  return buf[0];
}

int GetEOFFlag(string &buf)
{
  return buf[1];
}

void SendACK(int sockfd, struct sockaddr_in& serv_addr, int seq)
{
  // Write ACK number.
  char msg[4];
  strcpy(msg,"");
  msg[0] = (seq) & 0xFF;
  msg[1] = (seq >> 8) & 0xFF;
  msg[2] = (seq >> 16) & 0xFF;
  msg[3] = (seq >> 24) & 0xFF;

  sendto(sockfd, msg, strlen(msg), 0, (struct sockaddr *)&serv_addr, sizeof(serv_addr));
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

  vector<int> received(QUEUESIZE,0);
  vector<string> recvQueue(QUEUESIZE);
  int winSize = WINSIZE;
  int recBase = 0;

  struct sockaddr_in rec_addr; // receiver address
  socklen_t addrlen = sizeof(rec_addr);
  int recvlen; // # bytes received
  char buf[BUFSIZE]; 

  int seqNum;
  int EOFFlag;
  unsigned int recvWin = WINSIZE;
  int recvBase = 0;
  int recvEnd = recvBase + recvWin - 1;
  int recvFront = -recvWin;
  // Outputfile
  ofstream outfile;
  string outFilename("rec_");
  outFilename += filename;
  outfile.open(outFilename.c_str());

  // Receive File
  while (1) {
    recvlen = recvfrom(sockfd, buf, BUFSIZE, 0, (struct sockaddr *)&rec_addr, &addrlen);  
    
    string buffer(buf, recvlen);
//    cout << "receive msg = " << buffer << endl;
    seqNum = GetSeqNum(buffer);
    EOFFlag = GetEOFFlag(buffer);
    cout << "Seq: " << seqNum << "  EOFFlag: " << EOFFlag << endl;
    string payload = buffer.substr(2);
    cout << "Payload: " << payload << endl;
   
    int c;
    scanf("%d", &c);   
    // Seq is in [rcv_base, rcv_base+N-1]
    if ((seqNum >= recvBase) && (seqNum <= recvEnd)) {
      // Write queue buffer
      received[seqNum] = (EOFFlag? 2: 1);
      recvQueue[seqNum].assign(payload);
      // Send ACK.
      SendACK(sockfd, serv_addr, seqNum);
      // Write payload to file if recvBase is received.
      if (seqNum == recvBase) {
        while (received[recvBase]) {
          // Write data to file.
          outfile << recvQueue[recvBase];
          if (received[recvBase] == 2) {
            // Received all data.
            outfile.close();
            cout << "File transfer complete." << endl;
            return 0;
          }
          received[seqNum] = 0;
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
