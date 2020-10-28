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

#define BUF_SIZE 1024*1024
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
    header_t header;
    header_t ack_header;
    ack_header.ack = 0;
    int numBytes;
    int data_size;
    // int buf_top = 0;
    // int buf_len = 0;
    // int buf_seq = 0;

    while(1){
      /* Get data */
      if((numBytes = recvfrom(s, window, WINDOW_SIZE-1, 0, (struct sockaddr*)&si_other, &fromlen)) == -1){
        diep("recvfrom");
      }
      memcpy(&header, window, HEADER_SIZE);

      /* Check for FIN */
      if(header.flags == 1) break;

      data_size = numBytes - HEADER_SIZE;

      /* Write to file */
      // fseek(fp, header.seq, SEEK_SET);
      // fwrite(window + HEADER_SIZE, 1, data_size, fp);

      /* Send ACK */
      if(header.seq == ack_header.ack){
        ack_header.ack = header.seq + data_size;
        fwrite(window + HEADER_SIZE, 1, data_size, fp);
      }
      memcpy(buf, &ack_header, HEADER_SIZE);
      if(sendto(s, buf, HEADER_SIZE, 0, (struct sockaddr*)&si_other, slen) == -1){
        diep("sendto");
      }
    }

    /* Close */
    fclose(fp);
    printf("Beginning FIN\n");

    if(sendto(s, &ack_header, HEADER_SIZE, 0, (struct sockaddr*)&si_other, slen) == -1){
      diep("sendto");
    }
    ack_header.flags = 1;
    if(sendto(s, &ack_header, HEADER_SIZE, 0, (struct sockaddr*)&si_other, slen) == -1){
      diep("sendto");
    }

    if(recvfrom(s, window, WINDOW_SIZE-1, 0, (struct sockaddr*)&si_other, &fromlen) == -1){
      diep("recvfrom");
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
