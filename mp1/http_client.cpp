#include <iostream>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/wait.h>
#include <signal.h>
#include <fstream>

using namespace std;

#define PORT "80"
#define MAXBUFSIZE 1000000

int main(int argc, char *argv[]){
  int sockfd, nbytes = 0;
	struct addrinfo hints, *servinfo;
	int rv;
  string path;
  string hostname;
  string port;
  string http_request;
  char buf[MAXBUFSIZE];
  int flag = 0;
  ofstream file;
  int response;

  // Parse the argument to get port, hostname, and file path
  for(int i = 7; i < strlen(argv[1]); i++){
    if(argv[1][i] == '/' || flag == 2){
      flag = 2;
      path.push_back(argv[1][i]);
    } else if(argv[1][i] == ':' || flag == 1){
      flag = 1;
      if(argv[1][i] == ':') continue;
      port.push_back(argv[1][i]);
    } else if(flag == 0){
      hostname.push_back(argv[1][i]);
    }
  }

  memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;

  if(port.empty()){
    port = PORT;
  }

  if ((rv = getaddrinfo(hostname.c_str(), port.c_str(), &hints, &servinfo)) != 0) {
		perror("addr");
		return 1;
	}

  if ((sockfd = socket(servinfo->ai_family, servinfo->ai_socktype, servinfo->ai_protocol)) == -1) {
    perror("socket");
    exit(1);
  }

  if (connect(sockfd, servinfo->ai_addr, servinfo->ai_addrlen) == -1) {
    close(sockfd);
    perror("connect");
    exit(1);
  }

	if (servinfo == NULL) {
		return 2;
	}

	freeaddrinfo(servinfo);

  // Create HTTP request
  http_request = "GET " + path + " HTTP/1.1\r\n" + "Host: " + hostname + ":" + port
  + "\r\n" + "Connection: Close\r\n\r\n";

  cout << "Sending request\n";

  // Send HTTP request
  while((response = send(sockfd, http_request.c_str(), strlen(http_request.c_str()), 0)) > 0){
    nbytes += response;
    if(nbytes >= strlen(http_request.c_str())) break;
  }
  if(response == -1){
    perror("send");
    exit(1);
  }

  cout << "Downloading...\n";

  file.open("output", ios::out | ios::binary);

  // Receive file
  nbytes = 0;
  int header = 0;
  flag = 0;
  // while((response = recv(sockfd, buf, MAXBUFSIZE-1, 0)) > 0){
  //   for(int i = 0; i < response; i++){
  //     // Process header
  //     if(buf[i] == '\n' && (flag == 0 || flag == 1 ) && !header){
  //       flag++;
  //       if(flag == 2) header = 1;
  //     } else if(flag == 2){
  //       file.write(buf+i, 1);
  //     }
  //   }
  //   nbytes += response;
  // }
  response = recv(sockfd, buf, MAXBUFSIZE-1, 0);
  if(response == -1){
    perror("recv");
    exit(1);
  }
  for(int i = 0; i < response; i++){
    // Process header
    if(buf[i] == '\n' && (flag == 0 || flag == 1 ) && !header){
      flag++;
      if(flag == 2) header = 1;
    } else if(flag == 2){
      file.write(buf+i, 1);
    }
  }
  nbytes += response;

  file.close();

  cout << "Downloaded: " << nbytes << " bytes\n";

  close(sockfd);

  return 0;
}
