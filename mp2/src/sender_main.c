/*
 * File:   sender_main.c
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
#include <sys/stat.h>
#include <signal.h>
#include <string.h>
#include <sys/time.h>
#include <errno.h>

#define MSS 1024
#define HEADER_SIZE 12
#define BUF_SIZE 1024*1024
#define MSG_SIZE 1024*1024
#define BETA 0.75
#define ALPHA 0.125
#define TRANSMIT 0
#define RETRANSMIT 1

typedef struct h{
  int seq;
  int ack;
  short flags;
  short window;
} header_t;

typedef enum
{
  S_S, C_A, F_R
} state_t;

struct sockaddr_in si_other;
int s, slen;

int dupACKcnt = 0;
int cw = 1;
double cw_f = 0;
int sst = 64;
int base = 0;
int lastSent = -1;
state_t cur_state = S_S;
int eRTT = 20000;
int dRTT = 1;
int sRTT = 0;
uint64_t diff[BUF_SIZE];
uint64_t sent[BUF_SIZE];
uint64_t end[BUF_SIZE];
int q[BUF_SIZE];
int tIdx = 0;
int lastIdx = 0;
uint64_t prevTime = 0;
struct timeval t;

void diep(char *s) {
    perror(s);
    exit(1);
}

void sendMSG(int retransmit, FILE *fp, int bytesToTransfer, int reseq){
  char msg[MSG_SIZE];
  int numRead;
  int numBytes;
  int packet_size;
  header_t header;
  header.ack = 69;
  header.flags = 0;
  header.window = 0;

  /* Send packet(s) */
  for(int i = 0; i < cw; i++){
    // int seq = retransmit ? reseq : base + MSS*i;
    int seq = base + MSS*i;
    if(seq <= lastSent && !retransmit) continue;
    if(seq > bytesToTransfer) break;

    /* Create header */
    header.seq = seq;
    memcpy(msg, &header, HEADER_SIZE);  // Copy header to buffer

    /* Read from file */
    if(seq != SEEK_CUR) fseek(fp, seq, SEEK_SET);
    numRead = fread(msg + HEADER_SIZE, 1, MSS, fp);
    packet_size = (seq + numRead) < bytesToTransfer ? numRead : bytesToTransfer - seq;

    printf("seq: %d\n", seq);

    if((numBytes = sendto(s, msg, packet_size + HEADER_SIZE, 0, (struct sockaddr*)&si_other, slen)) == -1){
      diep("sendto");
    }

    gettimeofday(&t, NULL);
    diff[tIdx] = ((t.tv_sec*1000000) + t.tv_usec) - prevTime;
    sent[tIdx] = ((t.tv_sec*1000000) + t.tv_usec);
    q[tIdx] = seq;
    prevTime = sent[tIdx++];

    if(!retransmit) lastSent = seq; // Update pending packets
    // if(i == 0) retransmit = 0;
  }

  return;
}

void set_timer(struct timeval *timeout, int is_dup){
  gettimeofday(&t, NULL);
  uint64_t cur_time = (t.tv_sec*1000000) + t.tv_usec;
  if(is_dup){
    timeout->tv_sec = ((eRTT + 4*dRTT) - (cur_time - sent[lastIdx])) / 1000000;
    timeout->tv_usec = ((eRTT + 4*dRTT) - (cur_time - sent[lastIdx])) % 1000000;
  } else {
    sRTT = cur_time - sent[lastIdx];
    eRTT = (1 - ALPHA)*eRTT + ALPHA*sRTT;
    dRTT = (1 - BETA)*dRTT + BETA*abs(eRTT - sRTT);
    timeout->tv_sec = ((eRTT + 4*dRTT) - sRTT + diff[lastIdx]) / 1000000;
    timeout->tv_usec = ((eRTT + 4*dRTT) - sRTT + diff[lastIdx]) % 1000000;
  }

  // printf("%d", timeout->tv_sec);
  if(setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, (char *)timeout, sizeof(struct timeval)) == -1){
    diep("setsockopt");
  }

  return;
}

void reliablyTransfer(char* hostname, unsigned short int hostUDPport, char* filename, unsigned long long int bytesToTransfer) {
    // Open the file
    FILE *fp;
    fp = fopen(filename, "rb");
    if (fp == NULL) {
        printf("Could not open file to send.");
        exit(1);
    }

    slen = sizeof (si_other);

    if ((s = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) == -1)
        diep("socket");

    memset((char *) &si_other, 0, sizeof (si_other));
    si_other.sin_family = AF_INET;
    si_other.sin_port = htons(hostUDPport);
    if (inet_aton(hostname, &si_other.sin_addr) == 0) {
        fprintf(stderr, "inet_aton() failed\n");
        exit(1);
    }

    /* Send data and receive acknowledgements on s */
    int response;
    socklen_t fromlen = sizeof(si_other);
    char buf[BUF_SIZE];
    header_t header;
    struct timeval timeout;

    /* Send first packet */
    sendMSG(TRANSMIT, fp, bytesToTransfer, 0);
    set_timer(&timeout, 0);

    /* Transmission */
    while(base < bytesToTransfer){
      /* Handle ACKs */
      response = recvfrom(s, buf, BUF_SIZE-1, 0, (struct sockaddr*)&si_other, &fromlen);

      /* Parse ACK */
      memcpy(&header, buf, HEADER_SIZE);
      printf("ack: %d\n", header.ack);
      /* Handle event */
      if(cw >= sst) cur_state = C_A;

      if(response == -1){ // TIMEOUT
        printf("here\n");
        sst = cw / 2;
        cw = 1;
        cw_f = 0;
        dupACKcnt = 0;
        sendMSG(RETRANSMIT, fp, bytesToTransfer, q[lastIdx]);
        set_timer(&timeout, 0);
        lastIdx++;
        cur_state = S_S;
      }
      else if(header.ack > base){ // NEW_ACK
        switch(cur_state){
          case S_S:
            cw += (header.ack - base) / MSS;
            break;
          case C_A:
            cw_f += ((double)(header.ack - base) / (double)MSS) / (double)cw;
            cw += cw_f;
            if(cw_f == 1) cw_f = 0;
            break;
          case F_R:
            cw = sst;
            cur_state = C_A;
            break;
        }
        dupACKcnt = 0;
        lastIdx += (header.ack - base) / MSS;
        base = header.ack;
        // printf("base: %d\n", base);
        if(base >= bytesToTransfer) break;
        sendMSG(TRANSMIT, fp, bytesToTransfer, 0);
        set_timer(&timeout, 0);
      }
      else if(header.ack == base){ // DUP_ACK
        switch(cur_state){
          case F_R:
            cw += 1;
            sendMSG(TRANSMIT, fp, bytesToTransfer, 0);
            break;
          case S_S: case C_A:
            dupACKcnt++;
            set_timer(&timeout, 1);
            if(dupACKcnt == 3){
              sst = cw / 2;
              cw = sst + 3;
              cw_f = 0;
              sendMSG(RETRANSMIT, fp, bytesToTransfer, header.ack);
              cur_state = F_R;
            }
            break;
        }
      }
    }

    /* Close */
    fclose(fp);
    printf("Beginning FIN\n");

    /* Send FIN */
    header.flags = 1;
    memcpy(buf, &header, HEADER_SIZE);
    if(sendto(s, buf, HEADER_SIZE, 0, (struct sockaddr*)&si_other, slen) == -1){
      diep("sendto");
    }

    /* Receive ACK and FIN */
    timeout.tv_sec = 0;
    timeout.tv_usec = 500000;
    if(setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, (char *)&timeout, sizeof(timeout)) == -1){
      diep("setsockopt");
    }
    if(recvfrom(s, buf, BUF_SIZE-1, 0, (struct sockaddr*)&si_other, &fromlen) == -1){
      diep("recvfrom");
    }
    if(recvfrom(s, buf, BUF_SIZE-1, 0, (struct sockaddr*)&si_other, &fromlen) == -1){
      diep("recvfrom");
    }

    memcpy(buf, &header, HEADER_SIZE);
    if(sendto(s, buf, HEADER_SIZE, 0, (struct sockaddr*)&si_other, slen) == -1){
      diep("sendto");
    }

    timeout.tv_sec = 0;
    timeout.tv_usec = 5000;
    if(setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, (char *)&timeout, sizeof(timeout)) == -1){
      diep("setsockopt");
    }
    if(recvfrom(s, buf, BUF_SIZE-1, 0, (struct sockaddr*)&si_other, &fromlen) == -1){
      if(errno != EAGAIN && errno != EWOULDBLOCK) diep("recvfrom");
    }

    printf("Closing the socket\n");
    close(s);
    return;
}

int main(int argc, char** argv) {

    unsigned short int udpPort;
    unsigned long long int numBytes;

    if (argc != 5) {
        fprintf(stderr, "usage: %s receiver_hostname receiver_port filename_to_xfer bytes_to_xfer\n\n", argv[0]);
        exit(1);
    }
    udpPort = (unsigned short int) atoi(argv[2]);
    numBytes = atoll(argv[4]);

    reliablyTransfer(argv[1], udpPort, argv[3], numBytes);

    return (EXIT_SUCCESS);
}
