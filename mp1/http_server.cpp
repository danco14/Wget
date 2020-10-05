#include <iostream>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <string>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/wait.h>
#include <signal.h>
#include <fstream>

#define MAXBUFSIZE 1000000
#define BACKLOG 1000

using namespace std;

int main(int argc, char *argv[]){
  int sockfd, new_fd;
	struct addrinfo hints, *servinfo;
	struct sockaddr_storage their_addr;
	socklen_t sin_size;
	int yes=1;
	int rv;
  string response;
  int nbytes;
  ifstream file;
  string fname;
  char buf[MAXBUFSIZE];
  char temp[MAXBUFSIZE];

  if(argc != 2){
    exit(1);
  }

  memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_PASSIVE;

  if ((rv = getaddrinfo(NULL, argv[1], &hints, &servinfo)) != 0) {
    perror("addr");
    return 1;
	}

  if ((sockfd = socket(servinfo->ai_family, servinfo->ai_socktype, servinfo->ai_protocol)) == -1) {
    perror("socket");
    exit(1);
  }

  if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) == -1) {
    perror("opt");
    exit(1);
  }

  if (bind(sockfd, servinfo->ai_addr, servinfo->ai_addrlen) == -1) {
    close(sockfd);
    perror("bind");
    exit(1);
  }

  if (servinfo == NULL)  {
		return 2;
	}

  freeaddrinfo(servinfo);

  if (listen(sockfd, BACKLOG) == -1) {
		perror("listen");
		exit(1);
	}

  while(1){
		sin_size = sizeof(their_addr);
		new_fd = accept(sockfd, (struct sockaddr *)&their_addr, &sin_size);
		if (new_fd == -1) {
			perror("accept");
			continue;
		}

		if(!fork()){
      close(sockfd);
      if((nbytes = recv(new_fd, temp, MAXBUFSIZE-1, 0)) == -1){
        perror("recv");
      }

      int flag = 0;
      int j = 0;
      for(int i = 0; i < strlen(temp); i++){
        if(temp[i] == ' ' && flag == 0){
          flag = 1;
          continue;
        }
        if(flag && temp[i] == ' '){
          break;
        } else if(flag && temp[i] != '/'){
          fname.push_back(temp[i]);
        }
      }

      file.open(fname.c_str(), ios::in | ios::binary);
      file.seekg(0, file.end);
      int size = file.tellg();
      int total_sent = 0;
      file.seekg(0, file.beg);

      // File is either found or not found
      if(file.is_open()){
        string http_header = "HTTP/1.1 200 OK\r\n\r\n";
        int header = 0;
        // Loop to send package in chunks
        while(total_sent < size){
          int chunk = ((size - total_sent) > MAXBUFSIZE) ? MAXBUFSIZE : (size - total_sent);
          int length = !header ? http_header.length() + chunk : chunk;
          file.read(buf, chunk);
          response = !header ? http_header : "";
          for(int i = 0; i < chunk; i++){
            response.push_back(buf[i]);
          }
          total_sent += chunk;
          if((nbytes = send(new_fd, response.c_str(), length, 0)) == -1){
            perror("send");
          }
          header = 1;
        }
      } else{
        response = "HTTP/1.1 404 Not Found\r\n\r\n";
        response.push_back('H');
        response.push_back('I');
        if((nbytes = send(new_fd, response.c_str(), response.length(), 0)) == -1){
          perror("send");
        }
      }
      file.close();
			close(new_fd);
			exit(0);
		}
		close(new_fd);
	}

  return 0;
}
