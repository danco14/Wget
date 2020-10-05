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
  int sockfd, nbytes;
	struct addrinfo hints, *servinfo, *p;
	int rv;
	char s[INET6_ADDRSTRLEN];
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

  // loop through all the results and connect to the first we can
	for(p = servinfo; p != NULL; p = p->ai_next) {
		if ((sockfd = socket(p->ai_family, p->ai_socktype,
				p->ai_protocol)) == -1) {
			perror("client: socket");
			continue;
		}

		if (connect(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
			close(sockfd);
			perror("client: connect");
			continue;
		}
		//printf("%d\n",sockfd);l
		break;
	}

	if (p == NULL) {
		fprintf(stderr, "client: failed to connect\n");
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
  // int header = 0;
  flag = 0;
  while((response = recv(sockfd, buf, MAXBUFSIZE-1, 0)) > 0){
    for(int i = 0; i < response; i++){
      // Process header
      if(buf[i] == '\n' && buf[i-1] == '\r' && buf[i-2] == '\n' && buf[i-3] == '\r' && !flag){
        // flag++;
        flag = 1;
        // if(flag == 2) header = 1;
      } else if(flag == 1){
        file.write(buf+i, 1);
      }
    }
    nbytes += response;
  }
  response = recv(sockfd, buf, MAXBUFSIZE-1, 0);
  if(response == -1){
    perror("recv");
    exit(1);
  }
  // buf[response] = '\0';
  // for(int i = 0; i < response; i++){
  //   // Process header
  //   if(buf[i] == '\n' && buf[i-1] == '\r' && buf[i-2] == '\n' && buf[i-3] == '\r' && !flag){
  //     flag = 1;
  //   } else if(flag == 1){
  //     file.write(buf+i, 1);
  //   }
  // }
  // nbytes += response;

  file.close();

  cout << "Downloaded: " << nbytes << " bytes\n";

  close(sockfd);

  return 0;
}
