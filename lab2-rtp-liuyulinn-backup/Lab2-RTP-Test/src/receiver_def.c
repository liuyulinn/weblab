#include "util.h"
#include "receiver_def.h"
#include "rtp.h"
#define MAXNUM 1500
#define RECV_MAXNUM 1500*518

static int receiver_sock;
static struct sockaddr_in sender_addr;
static struct sockaddr_in tmpt_sender;
static uint32_t receiver_window_size;
static char receiver_buffer[MAXNUM];
static rtp_header_t* receiver_rtp;
char recv_storebuffer[RECV_MAXNUM];

/**
 * @brief 开启receiver并在所有IP的port端口监听等待连接
 * 
 * @param port receiver监听的port
 * @param window_size window大小
 * @return -1表示连接失败，0表示连接成功
 */
int initReceiver(uint16_t port, uint32_t window_size)
{
    // 创建一个socket 进行监听
    if((receiver_sock = socket(AF_INET, SOCK_DGRAM, 0)) < 0)
    {
        //printf("initSender: socket create failed!\n");
        return -1;
    }

    //设置socket address
    memset(&sender_addr, 0, sizeof(struct sockaddr_in));
    sender_addr.sin_family = AF_INET;
    sender_addr.sin_addr.s_addr = INADDR_ANY;
    sender_addr.sin_port = htons(port); //设置port
    socklen_t addr_len = sizeof(struct sockaddr_in);

    //将这个sock_address绑定到socket上
    int result;
    if((result = bind(receiver_sock, (struct sockaddr*)&sender_addr, sizeof(struct sockaddr))) < 0)
    {
        //printf("bind failed!\n");
        return -1;
    }

    // 接受信息，如果是connection，检查checksum，并返回ACK值
    if((result = recvfrom(receiver_sock, receiver_buffer, MAXNUM, 0, (struct sockaddr*)&tmpt_sender, &addr_len)) < 0)
    {
        //printf("receive error!\n");
        return -1;
    }
    receiver_buffer[result] = '\0';
    receiver_rtp = (rtp_header_t*)receiver_buffer;
    if((result = assert_checksum((void*)receiver_buffer, sizeof(rtp_header_t))) < 0) return -1; //检查checksum

    if(receiver_rtp->type != RTP_START) //是否是RTP_STAT
    {
        //printf("please connect first!\n");
        return -1;
    }
    else
    {
        set_rtp_header((void*)receiver_buffer, RTP_ACK, receiver_rtp->seq_num);
        receiver_rtp->checksum = compute_checksum(receiver_rtp, sizeof(rtp_header_t));
        if((result = rtp_sendto(receiver_sock, (void*)receiver_buffer, sizeof(rtp_header_t), 0, (struct sockaddr*)&tmpt_sender, sizeof(struct sockaddr))) < 0)
        {
            return -1;
        }
        //开始协议
        receiver_window_size = window_size;
        //printf("connection done!\n");
        return 0;
    }
}

int recvMessage(char* filename)
{

    // open file
    // char *newfile = malloc(strlen(filename) + 1);
    // memcpy(newfile, filename, strlen(filename));
    // newfile[strlen(filename)] = '\0';
    FILE *f = fopen(filename, "w");
    
    int expected_seq_num = 0;//期待收到的数据
    int recv[receiver_window_size + 1];//标记是否收到信息
    int i,j; //循环指标
    for(i = 0; i < (receiver_window_size + 1); ++i) recv[i] = 0;
    int end;
    int result;
    rtp_header_t* tmpt_receiver;

    socklen_t addr_len = sizeof(struct sockaddr_in);

    while(1)
    {
        end = 0;
        if((result = recvfrom(receiver_sock, receiver_buffer, MAXNUM, 0, (struct sockaddr*)&tmpt_sender, &addr_len)) < 0)
        {
            //printf("Receiver: receive error!\n");
            continue;
        }
        receiver_buffer[result] = '\0';
        receiver_rtp = (rtp_header_t*)receiver_buffer;
        if((result = assert_checksum((void*)receiver_buffer, result)) < 0) continue; //检查checksum

        if(receiver_rtp->type == RTP_DATA)
        {
            if(receiver_rtp->seq_num >= receiver_window_size + expected_seq_num); //大于滑动窗口，丢弃
            else if(receiver_rtp->seq_num < expected_seq_num) //可能是标记错乱
                receiver_rtp->seq_num = expected_seq_num;
            else if(receiver_rtp->seq_num == expected_seq_num)
            {
                memcpy(recv_storebuffer + expected_seq_num%(receiver_window_size+1) * MAXNUM, receiver_buffer, receiver_rtp->length + sizeof(rtp_header_t));//全部都存放
                ////printf("=================%d\n", receiver_rtp->length);
                recv[expected_seq_num % (receiver_window_size+1)] = 1; //缓存了的区域
                recv_storebuffer[(expected_seq_num % (receiver_window_size + 1)) * MAXNUM + receiver_rtp->length + sizeof(rtp_header_t)] = '\0';
                //printf("Receiver: ack %d recv result! expected %d\n", receiver_rtp->seq_num, expected_seq_num);

                for(i = 0; i < receiver_window_size; ++ i)
                {
                    if(recv[(expected_seq_num + i) % (receiver_window_size+1)] == 1)
                    {
                        ////printf("%s", recv_storebuffer + expected_seq_num%(receiver_window_size + 1) * MAXNUM);
                        tmpt_receiver = (rtp_header_t*)(recv_storebuffer +((expected_seq_num + i)%(receiver_window_size+1)*MAXNUM));
                        //printf("Receiver: print %d\n", tmpt_receiver->length);
                        for(j=0; j < (tmpt_receiver->length); ++j)
                        {
                            fputc(recv_storebuffer[((expected_seq_num + i)%(receiver_window_size+1)*MAXNUM) + sizeof(rtp_header_t) + j], f);
                        }
                        //f//printf(f, "%s", recv_storebuffer + (expected_seq_num)%(receiver_window_size+1)*MAXNUM); //一次读写
                        ////printf("the file is:%s\n", recv_storebuffer + (expected_seq_num)%(receiver_window_size+1)*MAXNUM);
                        ////printf("write done!\n");
                        recv[(expected_seq_num + i) % (receiver_window_size+1)] = 0; //写入数据后，缓存区重置
                    }
                    else
                    {
                        break;
                    }
                }   
                //printf("Receiver: update expected_seq_num previous: %d, now: %d\n", expected_seq_num, expected_seq_num + i);
                expected_seq_num = expected_seq_num + i;      
                
            }
            else //乱序到达，但是在滑动窗口内
            {
                memcpy(recv_storebuffer + receiver_rtp->seq_num%(receiver_window_size+1) * MAXNUM, receiver_buffer, receiver_rtp->length + sizeof(rtp_header_t));
                recv[receiver_rtp->seq_num % (receiver_window_size + 1)] = 1;
                recv_storebuffer[receiver_rtp->seq_num %(receiver_window_size+1) * MAXNUM + receiver_rtp->length + sizeof(rtp_header_t)] = '\0';
                //printf("Receiver: not in order: ack %d recv result! expected %d.\n", receiver_rtp->seq_num, expected_seq_num);
            }
            //发送回复消息
            set_rtp_header((void*)receiver_buffer, RTP_ACK, expected_seq_num);
            receiver_rtp->checksum = compute_checksum((void*)receiver_buffer, sizeof(rtp_header_t));
            if((result = rtp_sendto(receiver_sock, (void*)receiver_buffer, sizeof(rtp_header_t), 0, (struct sockaddr*)&tmpt_sender, sizeof(struct sockaddr))) < 0)
            {
                continue;//发送失败，说明网络存在问题
            }
        } 
        else if(receiver_rtp->type == RTP_END)
        {
            //set_rtp_header((void*)receiver_buffer, RTP_ACK, expected_seq_num);
            receiver_rtp->type = RTP_ACK;
            receiver_rtp->length = 0;
            receiver_rtp->checksum = 0;
            receiver_rtp->checksum = compute_checksum(receiver_rtp, sizeof(rtp_header_t));
            //printf("Receiver: receive end seq_num %d, expected seq_num %d\n", receiver_rtp->seq_num, expected_seq_num);
            if((result = rtp_sendto(receiver_sock, (void*)receiver_buffer, sizeof(rtp_header_t), 0, (struct sockaddr*)&tmpt_sender, sizeof(struct sockaddr))) < 0)
            {
                continue;
            }
            if(receiver_rtp->seq_num == expected_seq_num)
            {
                //printf("Receiver: send ACK end!\n");
                end = 1;
            }
        }
        if(end) break;
    }
    //printf("Receiver: END LOOPE!\n");
    fclose(f);
    return 0;
}

/**
 * @brief 用于接收数据并在接收完后断开RTP连接 (优化版本的RTP)
 * @param filename 用于接收数据的文件名
 * @return >0表示接收完成后到数据的字节数 -1表示出现其他错误
 */
int recvMessageOpt(char* filename)
{

    // open file
    // char *newfile = malloc(strlen(filename) + 1);
    // memcpy(newfile, filename, strlen(filename));
    // newfile[strlen(filename)] = '\0';
    FILE *f = fopen(filename, "w");
    
    int expected_seq_num = 0;//期待收到的数据
    int recv[receiver_window_size + 1];//标记是否收到信息
    int i,j; //循环指标
    for(i = 0; i < (receiver_window_size + 1); ++i) recv[i] = 0;
    int end;
    int result;
    rtp_header_t* tmpt_receiver;

    socklen_t addr_len = sizeof(struct sockaddr_in);

    while(1)
    {
        end = 0;
        if((result = recvfrom(receiver_sock, receiver_buffer, MAXNUM, 0, (struct sockaddr*)&tmpt_sender, &addr_len)) < 0)
        {
            //printf("Receiver: receive error!\n");
            continue;
        }
        receiver_buffer[result] = '\0';
        receiver_rtp = (rtp_header_t*)receiver_buffer;
        if((result = assert_checksum((void*)receiver_buffer, result)) < 0) continue; //检查checksum

        if(receiver_rtp->type == RTP_DATA)
        {
            if(receiver_rtp->seq_num >= receiver_window_size + expected_seq_num); //大于滑动窗口，丢弃
            else if(receiver_rtp->seq_num < expected_seq_num) //可能是标记错乱
                receiver_rtp->seq_num = expected_seq_num;
            else if(receiver_rtp->seq_num > expected_seq_num) //乱序到达
            {
                memcpy(recv_storebuffer + receiver_rtp->seq_num%(receiver_window_size+1) * MAXNUM, receiver_buffer, receiver_rtp->length + sizeof(rtp_header_t));//全部都存放
                ////printf("=================%d\n", receiver_rtp->length);
                recv[receiver_rtp->seq_num % (receiver_window_size+1)] = 1; //缓存了的区域
                recv_storebuffer[(receiver_rtp->seq_num % (receiver_window_size + 1)) * MAXNUM + receiver_rtp->length + sizeof(rtp_header_t)] = '\0';
                //printf("Receiver: ack %d recv result! expected %d\n", receiver_rtp->seq_num, expected_seq_num);

                //发送回复消息
                set_rtp_header((void*)receiver_buffer, RTP_ACK, receiver_rtp->seq_num);
                receiver_rtp->checksum = compute_checksum((void*)receiver_buffer, sizeof(rtp_header_t));
                if((result = rtp_sendto(receiver_sock, (void*)receiver_buffer, sizeof(rtp_header_t), 0, (struct sockaddr*)&tmpt_sender, sizeof(struct sockaddr))) < 0)
                {
                    continue;//发送失败，说明网络存在问题
                }
            }
            else//顺序到达
            {
                memcpy(recv_storebuffer + receiver_rtp->seq_num%(receiver_window_size+1) * MAXNUM, receiver_buffer, receiver_rtp->length + sizeof(rtp_header_t));//全部都存放
                ////printf("=================%d\n", receiver_rtp->length);
                recv[receiver_rtp->seq_num % (receiver_window_size+1)] = 1; //缓存了的区域
                recv_storebuffer[(receiver_rtp->seq_num % (receiver_window_size + 1)) * MAXNUM + receiver_rtp->length + sizeof(rtp_header_t)] = '\0';
                //printf("Receiver: ack %d recv result! expected %d\n", receiver_rtp->seq_num, expected_seq_num);

                //发送回复消息
                set_rtp_header((void*)receiver_buffer, RTP_ACK, receiver_rtp->seq_num);
                receiver_rtp->checksum = compute_checksum((void*)receiver_buffer, sizeof(rtp_header_t));
                if((result = rtp_sendto(receiver_sock, (void*)receiver_buffer, sizeof(rtp_header_t), 0, (struct sockaddr*)&tmpt_sender, sizeof(struct sockaddr))) < 0)
                {
                    continue;//发送失败，说明网络存在问题
                }

                //写入数据，并更新expected_seq_num
                for(i = 0; i < receiver_window_size; ++ i)
                {
                    if(recv[(expected_seq_num + i) % (receiver_window_size+1)] == 1)
                    {
                        ////printf("%s", recv_storebuffer + expected_seq_num%(receiver_window_size + 1) * MAXNUM);
                        tmpt_receiver = (rtp_header_t*)(recv_storebuffer +((expected_seq_num + i)%(receiver_window_size+1)*MAXNUM));
                        //printf("Receiver: print %d\n", tmpt_receiver->length);
                        for(j=0; j < (tmpt_receiver->length); ++j)
                        {
                            fputc(recv_storebuffer[((expected_seq_num + i)%(receiver_window_size+1)*MAXNUM) + sizeof(rtp_header_t) + j], f);
                        }
                        //f//printf(f, "%s", recv_storebuffer + (expected_seq_num)%(receiver_window_size+1)*MAXNUM); //一次读写
                        ////printf("the file is:%s\n", recv_storebuffer + (expected_seq_num)%(receiver_window_size+1)*MAXNUM);
                        ////printf("write done!\n");
                        recv[(expected_seq_num + i) % (receiver_window_size+1)] = 0; //写入数据后，缓存区重置
                    }
                    else
                    {
                        break;
                    }
                }   
                //printf("Receiver: update expected_seq_num previous: %d, now: %d\n", expected_seq_num, expected_seq_num + i);
                expected_seq_num = expected_seq_num + i;      
                
            }
        } 
        else if(receiver_rtp->type == RTP_END)
        {
            //set_rtp_header((void*)receiver_buffer, RTP_ACK, expected_seq_num);
            receiver_rtp->type = RTP_ACK;
            receiver_rtp->length = 0;
            receiver_rtp->checksum = 0;
            receiver_rtp->checksum = compute_checksum(receiver_rtp, sizeof(rtp_header_t));
            //printf("Receiver: receive end seq_num %d, expected seq_num %d\n", receiver_rtp->seq_num, expected_seq_num);
            if((result = rtp_sendto(receiver_sock, (void*)receiver_buffer, sizeof(rtp_header_t), 0, (struct sockaddr*)&tmpt_sender, sizeof(struct sockaddr))) < 0)
            {
                continue;
            }
            if(receiver_rtp->seq_num == expected_seq_num)
            {
                //printf("Receiver: send ACK end!\n");
                end = 1;
            }
        }
        if(end) break;
    }
    //printf("Receiver: END LOOPE!\n");
    fclose(f);
    return 0;
}


void terminateReceiver()
{
    // 释放socket
    //while(1);
    close(receiver_sock);
}
