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

#define MAXBUFSIZE 100000
#define BACKLOG 20

using namespace std;

void sigchld_handler(int s)
{
	while(waitpid(-1, NULL, WNOHANG) > 0);
}

int main(int argc, char *argv[]){
  int sockfd, new_fd;
	struct addrinfo hints, *servinfo, *p;
	struct sockaddr_storage their_addr;
	socklen_t sin_size;
	struct sigaction sa;
	int yes=1;
	int rv;
  string http_header;
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
		fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
		return 1;
	}

  // loop through all the results and bind to the first we can
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
		sin_size = sizeof their_addr;
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
      cout << fname;
      file.open(fname.c_str(), ios::in);
      if(file.is_open()){
        http_header = "HTTP/1.1 200 OK\r\n";
        file >> buf;
        response = http_header + buf + "\n";
        if((nbytes = send(new_fd, response.c_str(), strlen(response.c_str()), 0)) == -1){
          perror("send");
        }
      } else{
        http_header = "HTTP/1.1 404 Not Found\r\n\r\n";
        if((nbytes = send(new_fd, http_header.c_str(), strlen(http_header.c_str()), 0)) == -1){
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
