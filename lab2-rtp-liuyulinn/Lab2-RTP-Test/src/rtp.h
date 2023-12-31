#ifndef RTP_H
#define RTP_H

#include <stdint.h>
#include <netinet/in.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <stdlib.h>

#ifdef __cplusplus
extern "C" {
#endif

#define RTP_START 0
#define RTP_END   1
#define RTP_DATA  2
#define RTP_ACK   3

#define PAYLOAD_SIZE 1461

typedef struct __attribute__ ((__packed__)) RTP_header {
    uint8_t type;       // 0: START; 1: END; 2: DATA; 3: ACK
    uint16_t length;    // Length of data; 0 for ACK, START and END packets
    uint32_t seq_num;
    uint32_t checksum;  // 32-bit CRC
} rtp_header_t;


typedef struct __attribute__ ((__packed__)) RTP_packet {
    rtp_header_t rtp;
    char payload[PAYLOAD_SIZE];
} rtp_packet_t;

int assert_checksum(const void* buffer, size_t n_bytes);
void set_rtp_header(const void* buffer, uint8_t type, uint32_t seq_num);
int rtp_sendto(int sock, void* buffer, size_t len, int flag, struct sockaddr* sockaddr, socklen_t socklen);
// int rtp_recvfrom(int sock, void* buffer, size_t size, int flag, struct sockaddr* sockaddr, socklen_t socklen);


#ifdef __cplusplus
}
#endif

#endif //RTP_H
