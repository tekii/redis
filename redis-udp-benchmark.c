#include <arpa/inet.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>

#include "anet.h"
#include "sds.h"
#include "ae.h"

#define REDIS_UDP_PACKET_SIZE 65507

typedef struct udpPacketHdr {
    uint32_t reqid;
    uint8_t opcode;     /* 1 = Request, 2 = Reply, 3 = Req ack, 4 = Ack */
    uint8_t flags;      /* ack field if opcode == ACK */
    uint16_t datalen;   /* Data length, only for Request and Reply packets */
    uint16_t arity;     /* Number of arguments in request. Only for requests */
    uint16_t dbid;      /* DB ID. Only for requests */
} udpPacketHdr;

int udpsocket;
sds argv[1];
long long sent_packets = 0;
long long recv_packets = 0;

int sendUDPRequestVector(int s, uint32_t reqid, int dbid, int flags, char **argv, int argc) {
    unsigned char buf[REDIS_UDP_PACKET_SIZE];
    udpPacketHdr *hdr = (udpPacketHdr*) buf;
    unsigned char *p = buf + sizeof(udpPacketHdr);
    int room = REDIS_UDP_PACKET_SIZE-sizeof(udpPacketHdr);
    int j;

    hdr->reqid = htonl(reqid);
    hdr->opcode = 0x01;
    hdr->flags = flags;
    hdr->arity = htons(argc);
    hdr->dbid = htons(dbid);

    for (j = 0; j < argc; j++) {
        int arglen = sdslen(argv[j]);

        if (room < (2+arglen)) goto nobuf;
        p[0] = arglen>>8;
        p[1] = arglen&0xff;
        memcpy(p+2,argv[j],arglen);
        p += 2+arglen;
        room -= 2+arglen;
    }
    hdr->datalen = htons(p-buf-sizeof(udpPacketHdr));
    return write(s,buf,p-buf);

nobuf:
    errno = ENOBUFS;
    return -1;
}

int readUDPReply(int s) {
    unsigned char buf[REDIS_UDP_PACKET_SIZE];

    return read(s,buf,REDIS_UDP_PACKET_SIZE);
}

static void readHandler(aeEventLoop *el, int fd, void *privdata, int mask) {
    readUDPReply(fd);
    recv_packets++;
}

static void writeHandler(aeEventLoop *el, int fd, void *privdata, int mask) {
    sendUDPRequestVector(fd,1,0,0,argv,1);
    sent_packets++;
    if (!(sent_packets % 10000)) {
        printf("Sent: %lld, Recv: %lld\n", sent_packets, recv_packets);
    }
}

int main(void) {
    char err[ANET_ERR_LEN];
    int c = 0;
    aeEventLoop *el = aeCreateEventLoop();

    argv[0] = sdsnew("ping");
    
    udpsocket = anetUdpConnectedClient(err, "127.0.0.1", 6379);
    aeCreateFileEvent(el,udpsocket,AE_READABLE,readHandler,NULL);
    aeCreateFileEvent(el,udpsocket,AE_WRITABLE,writeHandler,NULL);

    aeMain(el);
}
