#include "rtp.h"
#include "util.h"
#define MAXNUM 1500

int assert_checksum(const void* buffer, size_t n_bytes){
    rtp_header_t* rtp = (rtp_header_t*) buffer;
    uint32_t this_checksum = rtp->checksum;
    uint32_t computed_checksum;

    rtp->checksum = 0;
    computed_checksum = compute_checksum(buffer, n_bytes);
    if(computed_checksum == this_checksum) return 0;
    else
    {
        //printf("checksum can't match!\n");
        return -1;
    }
}

//initalize rtp_header, return -1 if type not in [START, DATA, ACK, END], return 0 if success
void set_rtp_header(const void* buffer, uint8_t type, uint32_t seq_num)
{
    rtp_header_t* rtp = (rtp_header_t*) buffer;
    rtp->type = type;
    rtp->seq_num = seq_num;
    rtp->checksum = 0;
    rtp->length = 0;
}

int rtp_sendto(int sock, void* buffer, size_t len, int flag, struct sockaddr* sockaddr, socklen_t socklen)
{
    int n_bytes;
    if((n_bytes = sendto(sock, (void*)buffer,  len, 0, sockaddr, socklen)) != len)
    {
        printf("send error!\n");
        return -1;
    }
    return 0;
}


// int rtp_recvfrom(int sock, void* buffer, size_t size, int flag, struct sockaddr* sockaddr, socklen_t socklen)
// {
//     int n_bytes, result;
//     char tmp_buffer[MAXNUM];
//     if((n_bytes = recvfrom(sock, (void*)tmp_buffer, size, flag, sockaddr, &socklen)) < 0)
//     {
//         printf("receive error!\n");
//         return -1;
//     }
//     tmp_buffer[n_bytes] = '\0';
//     memcpy(buffer, (void*)tmp_buffer, n_bytes + 1);
//     //buffer = (void*)tmp_buffer;
//     // (char*) (buffer + n_bytes * sizeof(char)) = '\0'; //warning!
//     rtp_header_t* rtp = (rtp_header_t*)buffer;
//     if((result = assert_checksum((void*)buffer, n_bytes)) < 0) return -1;
//     return 0;
// }