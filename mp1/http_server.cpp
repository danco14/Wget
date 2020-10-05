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

#define MAXBUFSIZE 1024*1024
#define BACKLOG 1000

using namespace std;

void sigchld_handler(int s)
{
	while(waitpid(-1, NULL, WNOHANG) > 0);
}

int main(int argc, char *argv[]){
  int sockfd, new_fd;  // listen on sock_fd, new connection on new_fd
	struct addrinfo hints, *servinfo, *p;
	struct sockaddr_storage their_addr; // connector's address information
	socklen_t sin_size;
	struct sigaction sa;
	int yes=1;
	char s[INET6_ADDRSTRLEN];
	int rv;
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

  for(p = servinfo; p != NULL; p = p->ai_next) {
		if ((sockfd = socket(p->ai_family, p->ai_socktype,
				p->ai_protocol)) == -1) {
			perror("server: socket");
			continue;
		}

		if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes,
				sizeof(int)) == -1) {
			perror("setsockopt");
			exit(1);
		}

		if (bind(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
			close(sockfd);
			perror("server: bind");
			continue;
		}

		break;
	}

	if (p == NULL)  {
		fprintf(stderr, "server: failed to bind\n");
		return 2;
	}

  freeaddrinfo(servinfo);

  if (listen(sockfd, BACKLOG) == -1) {
		perror("listen");
		exit(1);
	}

  sa.sa_handler = sigchld_handler; // reap all dead processes
	sigemptyset(&sa.sa_mask);
	sa.sa_flags = SA_RESTART;
	if (sigaction(SIGCHLD, &sa, NULL) == -1) {
		perror("sigaction");
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

      file.open(fname.c_str(), ios::in);
      file.seekg(0, file.end);
      int size = file.tellg();
      // int total_sent = 0;
      file.seekg(0, file.beg);

      // File is either found or not found
      if(file.is_open()){
        string http_header = "HTTP/1.1 200 OK\r\n\r\n";
        // int header = 0;
        // Loop to send package in chunks
        // while(total_sent < size){
        string response;
        // cout << http_header.length();
        //   int chunk = ((size - total_sent) > (MAXBUFSIZE-1)) ? (MAXBUFSIZE-1) : (size - total_sent);
        //   if(!header){
        //     response = http_header;
        //     chunk -= http_header.length();
        //   }
        //   int length = !header ? http_header.length() + chunk : chunk;
        //   file.read(buf, chunk);
        //   for(int i = 0; i < chunk; i++){
        //     response.push_back(buf[i]);
        //   }
        //   total_sent += length - (length - chunk);
        //   if((nbytes = send(new_fd, response.c_str(), length, 0)) == -1){
        //     perror("send");
        //   }
        //   header = 1;
        // }
        response = http_header;
        file.read(buf, MAXBUFSIZE-1);
        for(int i = 0; i < file.gcount(); i++){
          response.push_back(buf[i]);
        }

        if((nbytes = send(new_fd, response.c_str(), http_header.length() + file.gcount(), 0)) == -1){
            perror("send");
          }
      } else{
        string response = "HTTP/1.1 404 Not Found\r\n\r\nError:";
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
