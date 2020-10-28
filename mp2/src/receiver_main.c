/*
 * File:   receiver_main.c
 * Author:
 *
 * Created on
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <pthread.h>
#include <errno.h>

#define BUF_SIZE 4*1024*1024
#define MSS 8192
#define BUF_LEN BUF_SIZE / MSS
#define WINDOW_SIZE 1024*1024
#define HEADER_SIZE 12

struct sockaddr_in si_me, si_other;
int s, slen;

typedef struct h{
  int seq;
  int ack;
  short flags;
  short window;
} header_t;

void diep(char *s) {
    perror(s);
    exit(1);
}

void reliablyReceive(unsigned short int myUDPport, char* destinationFile) {
    slen = sizeof (si_other);

    if ((s = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) == -1)
        diep("socket");

    memset((char *) &si_me, 0, sizeof (si_me));
    si_me.sin_family = AF_INET;
    si_me.sin_port = htons(myUDPport);
    si_me.sin_addr.s_addr = htonl(INADDR_ANY);
    printf("Now binding\n");
    if (bind(s, (struct sockaddr*) &si_me, sizeof (si_me)) == -1)
        diep("bind");

  	/* Now receive data and send acknowledgements */
    FILE *fp;
    fp = fopen(destinationFile, "wb");

    socklen_t fromlen = sizeof(si_other);
    char window[WINDOW_SIZE];
    char buf[BUF_SIZE];
    int slots_1[BUF_LEN];
    int slots_2[BUF_LEN];
    int *cur_slot;
    int *next_slot;
    header_t header;
    header_t ack_header;
    ack_header.ack = 0;
    int numBytes;
    int data_size;
    int idx;
    int turn = 0;
    for(int i = 0; i < BUF_LEN; i++){
      slots_1[i] = 0;
      slots_2[i] = 0;
    }

    while(1){
      /* Get data */
      if((numBytes = recvfrom(s, window, WINDOW_SIZE-1, 0, (struct sockaddr*)&si_other, &fromlen)) == -1){
        diep("recvfrom");
      }
      memcpy(&header, window, HEADER_SIZE);

      /* Check for FIN */
      if(header.flags == 1) break;

      data_size = numBytes - HEADER_SIZE;

      if(header.seq >= ack_header.ack){
        idx = (header.seq - ack_header.ack) / MSS;
        cur_slot = turn ? slots_1 : slots_2;

        if(cur_slot[idx] == 0){
          cur_slot[idx] = data_size;
          memcpy(buf + (header.seq - ack_header.ack), window + HEADER_SIZE, data_size);
        }
      }

      /* Send ACK */
      if(header.seq == ack_header.ack){
        int size = 0;
        int i;
        for(i = 0; i < BUF_LEN; i++){
          if(cur_slot[i]){
            ack_header.ack += cur_slot[i];
            size += cur_slot[i];
          } else{
            break;
          }
        }

        fwrite(buf, 1, size, fp);
        memcpy(buf, buf + size, BUF_SIZE - size);
        // memcpy(slots, slots + i*sizeof(int), (BUF_LEN - i)*sizeof(int));
        next_slot = turn ? slots_2 : slots_1;

        for(int j = 0; j < BUF_LEN; j++){
          if(j < (BUF_LEN - i))
            next_slot[j] = cur_slot[j + i];
          else
            next_slot[j] = 0;
        }

        turn = (turn + 1) % 2;
      }

      memcpy(window, &ack_header, HEADER_SIZE);
      if(sendto(s, window, HEADER_SIZE, 0, (struct sockaddr*)&si_other, slen) == -1){
        diep("sendto");
      }
    }

    /* Close */
    fclose(fp);
    printf("Beginning FIN\n");

    struct timeval t;
    t.tv_sec = 0;
    t.tv_usec = 500000;
    if(setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, (char *)&t, sizeof(t)) == -1){
      diep("setsockopt");
    }

    while(1){
      if(sendto(s, &ack_header, HEADER_SIZE, 0, (struct sockaddr*)&si_other, slen) == -1){
        diep("sendto");
      }
      ack_header.flags = 1;
      if(sendto(s, &ack_header, HEADER_SIZE, 0, (struct sockaddr*)&si_other, slen) == -1){
        diep("sendto");
      }

      if(recvfrom(s, window, WINDOW_SIZE-1, 0, (struct sockaddr*)&si_other, &fromlen) == -1){
        if(errno != EAGAIN && errno != EWOULDBLOCK) diep("recvfrom");
        continue;
      }
      break;
    }

    close(s);
    printf("%s received.", destinationFile);
    return;
}

int main(int argc, char** argv) {

    unsigned short int udpPort;

    if (argc != 3) {
        fprintf(stderr, "usage: %s UDP_port filename_to_write\n\n", argv[0]);
        exit(1);
    }

    udpPort = (unsigned short int) atoi(argv[1]);

    reliablyReceive(udpPort, argv[2]);
}
