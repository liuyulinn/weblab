#include "util.h"
#include "sender_def.h"
#include "rtp.h"
#define MAXNUM 1500
#define SND_MAXNUM 1500*518

static uint32_t sender_window_size;
static int sender_sock;
static struct sockaddr_in receiver_addr;
static struct sockaddr_in tmpt_receiver;
static char sender_buffer[MAXNUM];
rtp_header_t* sender_rtp;
static uint32_t sender_seq_num;
char snd_storebuffer[SND_MAXNUM]; //滑动窗口的缓冲区

/**
 * @brief 用于建立RTP连接
 * @param receiver_ip receiver的IP地址
 * @param receiver_port receiver的端口
 * @param window_size window大小
 * @return -1表示连接失败，0表示连接成功
 **/

int initSender(const char* receiver_ip, uint16_t receiver_port, uint32_t window_size)
{
    //建立socket
    if((sender_sock = socket(AF_INET, SOCK_DGRAM, 0)) < 0){
        //printf("initSender: socket create failed!\n");
        return -1;
    }
    sender_window_size = window_size; // 记录window_size大小
    
    memset(&receiver_addr, 0, sizeof(receiver_addr));
    receiver_addr.sin_family = AF_INET;
    receiver_addr.sin_port = htons(receiver_port);

    if(inet_pton(AF_INET, receiver_ip, &receiver_addr.sin_addr) <= 0)
    {
        //printf("initSender: conversion IP from text to binary form failed!\n");
        return -1;
    }

    socklen_t addr_len = sizeof(struct sockaddr_in);

    //发送START，等待相同seq_numd的ACK报文，成功返回0，失败返回-1
    srand(time(NULL)); //随机数种子
    sender_seq_num = (uint32_t)rand();

    // START报文
    sender_rtp = (rtp_header_t*)sender_buffer;
    set_rtp_header((void*)sender_buffer, RTP_START, sender_seq_num);
    sender_rtp->checksum = compute_checksum((void*)sender_buffer, sizeof(rtp_header_t));

    //发送START报文
    int result;
    
    if((result = rtp_sendto(sender_sock, (void*)sender_buffer, sizeof(rtp_header_t), 0, (struct sockaddr*)&receiver_addr, sizeof(struct sockaddr))) < 0)
    {
        return -1; //传输出现问题
    }
    //printf("successful send!\n");

    struct timeval timeinterval;
    timeinterval.tv_sec = 0;
    timeinterval.tv_usec = 100;
    fd_set set;
    FD_ZERO(&set);
    FD_SET(sender_sock, &set);


    while(1)
    {
        result = select(sender_sock + 1, &set, NULL, NULL, &timeinterval);
        if(result < 0)
        {
            //printf("unknown error!\n");
        }
        else if(result == 0)
        {
            //printf("init_timeout!\n");
            goto bad;
        }
        if((result = recvfrom(sender_sock, sender_buffer, MAXNUM, 0, (struct sockaddr*)&tmpt_receiver, &addr_len))< 0)
        {
            return -1; //传输中出现问题
        }
        sender_buffer[result] = '\0';
        sender_rtp = (rtp_header_t*)sender_buffer;
        //printf("ack:  %d %d\n", sender_seq_num, sender_rtp->seq_num);
        if((result = assert_checksum((void*)sender_buffer, sizeof(rtp_header_t))) < 0) goto bad; //检查checksum

        if(sender_rtp->type != RTP_ACK)
        {
            //printf("type error!\n");
            goto bad;
        }
        else if(sender_rtp->seq_num != sender_seq_num)
        {
            //printf("seq_num not match!\n");
            goto bad;
        }
        else
        {
            //printf("connection done!\n");
            ////printf("window_size: %d", sender_window_size);
            break;
        }
    }
    return 0;

bad:
    set_rtp_header((void*)sender_buffer, RTP_END, sender_seq_num + 1);
    sender_rtp = (rtp_header_t*)sender_buffer;
    sender_rtp->checksum = compute_checksum((void*)sender_buffer, sizeof(rtp_header_t));
    if((result = rtp_sendto(sender_sock, (void*)sender_buffer, sizeof(rtp_header_t), 0, (struct sockaddr*)&receiver_addr, sizeof(struct sockaddr))) < 0)
    {
        return -1; //传输出现问题
    }
    return -1;
}

/**
 * @brief 用于发送数据 (优化版本的RTP)
 * @param message 要发送的文件名
 * @return -1表示发送失败，0表示发送成功
 **/
int sendMessage(const char* message)
{
    //打开message
    //char* new_message = malloc(strlen(message) + 1); //warning？？？
    ////printf("%s", new_message);
    //memcpy(new_message, message, strlen(message));
    //ew_message[strlen(message)] = '\0';
    //FILE *f = fopen(new_message, "r");
    FILE *f = fopen(message, "r");
    //FILE *f = fopen("CMakeLists.txt", "r");
    //FILE *f = fopen("testfile", "r");
    ////printf("%s", message);
    ////printf("%s", new_message);

    sender_seq_num = 0; //初始化seq_num
    int unacked_seq_num = 0; 
    int result; //发送或接收是否成功
    int listen_result; //接受信号

    char* databuffer = malloc(MAXNUM);

    char c; //从file中一个字节一个字节的读取
    int count = 0; //表明已经读取到了哪个位置 
    int i;
    int end; //标记循环结束
    struct timeval timeinterval; //计时器
    fd_set set; 
    socklen_t addr_len = sizeof(struct sockaddr_in);
    rtp_header_t* tmp_sender_rtp;

    //printf("******\n");
    // while((c = fgetc(f)) != EOF)
    // {
    //     //printf("%c", c);
    // }
    // //printf("\n");
    //char package[]
    c = fgetc(f);
    while(1)
    {
        ////printf("%c", c);
        int end = 0;
        databuffer[count] = c;
        if(!feof(f)) count ++;

        if(count == PAYLOAD_SIZE -1 || (feof(f) && count != 0)) //将这一段打包发送出去
        {
            int fileend = feof(f);
            databuffer[count] = '\0';
            ////printf("\n");
            ////printf("count:%d, file:%s\n", count, databuffer);
            while(1)
            {
                //printf("------------ send a new package -----------\n");
                ////printf("sender_seq_num: %d, unacked_seq_num: %d, window_size: %d\n", sender_seq_num, unacked_seq_num, sender_window_size);
                while(1)//接收过程，尝试接受一个
                {
                    if((unacked_seq_num == sender_seq_num)) 
                    {
                        //printf("empty in window!\n");
                        break; //说明滑动窗口是空的
                    }
                    FD_ZERO(&set);
                    FD_SET(sender_sock, &set);
                    listen_result = select(sender_sock + 1, &set, NULL, NULL, &timeinterval);
                    //printf("listen_result %d, in trying to recv.\n", listen_result);
                    if(listen_result < 0) printf("error!\n"); //不知道什么情况
                    else if(listen_result == 0)//超时
                    {
                        //printf("send_timeout! resend now!\n");
                        //所有在滑动窗口内的数据都重新发送
                        //printf("unacked %d, sender %d\n", unacked_seq_num, sender_seq_num);
                        for(i = 0; i < (sender_seq_num + sender_window_size +1 - unacked_seq_num) % (sender_window_size + 1); ++i)
                        {
                            //printf("resend %d!\n", i + unacked_seq_num);
                            tmp_sender_rtp = (rtp_header_t*)(snd_storebuffer + (unacked_seq_num + i) %(sender_window_size+1) * MAXNUM);
                            //sender_rtp->length = tmp_sender_rtp->length;
                            //printf("resent seq_num %d, length %d\n", tmp_sender_rtp->seq_num, tmp_sender_rtp->length);
                            memcpy((void*)sender_buffer, (void*)(snd_storebuffer + (unacked_seq_num + i) %(sender_window_size+1) * MAXNUM), tmp_sender_rtp->length + sizeof(rtp_header_t));
                            //sender_rtp = (rtp_header_t*)sender_buffer;
                            //sender_rtp->checksum = compute_checksum((void*)sender_buffer, tmp_sender_rtp->length + sizeof(rtp_header_t));
                            if((result = rtp_sendto(sender_sock, (void*)sender_buffer, sizeof(rtp_header_t) + tmp_sender_rtp->length, 0, (struct sockaddr*)&receiver_addr, sizeof(struct sockaddr))) < 0)
                            {
                                continue;
                            }
                        }
                        //printf("reset timeinterval!\n");
                        timeinterval.tv_sec = 0; //计时器重置
                        timeinterval.tv_usec = 100;
                        //continue;
                    }
                    else// 收到包了
                    {
                        //receive results from receiver
                        if((result = recvfrom(sender_sock, sender_buffer, MAXNUM, 0, (struct sockaddr*)&tmpt_receiver, &addr_len)) < 0)
                        {
                            continue;//传输出现问题
                        }
                        sender_buffer[result] = '\0';
                        sender_rtp = (rtp_header_t*)sender_buffer;
                        //printf("Receiver: wish to receive %d\n", sender_rtp->seq_num);
                        if((result = assert_checksum((void*)sender_buffer, sizeof(rtp_header_t))) < 0) continue; //检查checksum
                        if(sender_rtp->type != RTP_ACK)
                        {
                            //printf("type error!\n");
                            continue;
                        }
                        else if(sender_rtp->seq_num >= unacked_seq_num + 1)
                        {
                            //printf("window size update! previous unacked %d, now unacked %d\n", unacked_seq_num, sender_rtp->seq_num);
                            unacked_seq_num = sender_rtp->seq_num;
                            //printf("reset timeinterval!\n");
                            timeinterval.tv_sec = 0; //计时器重置
                            timeinterval.tv_usec = 100;
                            break;//跳出尝试接受的loope
                        }
                    }
                    if(sender_seq_num < (unacked_seq_num + sender_window_size))
                    {
                        //printf("not receive, but still have sender window!\n");
                        break;
                    }
                }
                //尝试发送
                if(sender_seq_num < unacked_seq_num)//不知道什么情况，应该有问题
                {
                    sender_seq_num = unacked_seq_num;
                    //printf("unknown error occur!\n");
                    return -1;
                }
                else if(sender_seq_num < (unacked_seq_num + sender_window_size)) //可以发送
                {
                    set_rtp_header((void*)sender_buffer, RTP_DATA, sender_seq_num);
                    sender_rtp = (rtp_header_t*)sender_buffer;
                    memcpy((void*)sender_buffer + sizeof(rtp_header_t), (void*)databuffer, count);
                    sender_rtp->length = count;
                    ////printf("%s\n", databuffer);
                    sender_rtp->checksum = compute_checksum((void*)sender_buffer, count + sizeof(rtp_header_t));

                    if((result = rtp_sendto(sender_sock, (void*)sender_buffer, count + sizeof(rtp_header_t), 0, (struct sockaddr*)&receiver_addr, sizeof(struct sockaddr))) < 0)
                    {
                        return -1; //传输出现问题
                    }
                    if(sender_seq_num == 167)
                    {
                        //printf("%s", databuffer);
                    }
                    
                    memcpy(snd_storebuffer + (sender_seq_num % (sender_window_size+1)) * MAXNUM, sender_buffer, count + sizeof(rtp_header_t)); //存储到滑动窗口，把头也存了
                    //printf("cpy to storebuffer.\n");
                    if(sender_seq_num == unacked_seq_num) //第一次开始滑动窗口
                    {
                        timeinterval.tv_sec = 0;
                        timeinterval.tv_usec = 100;
                        //printf("reset timeinterval!\n");
                    }
                    ++ sender_seq_num;
                    //printf("send: %d, count = %d, unacked = %d, fileend = %d\n", sender_seq_num, count, unacked_seq_num, fileend);
                    count = 0;
                    break;
                }
            }
        }
        else if(feof(f) && count == 0)//数据已经读取完毕，发送END
        {
            //printf("Sender: Let's send end!\n");
            //先确保数据传输完毕
            while(sender_seq_num - unacked_seq_num > 1)//接收过程
            {
                FD_ZERO(&set);
                FD_SET(sender_sock, &set);
                listen_result = select(sender_sock + 1, &set, NULL, NULL, &timeinterval);
                if(listen_result < 0); //printf("error!\n");
                else if(listen_result == 0)//超时
                {
                    //printf("Sender: EOF_send_timeout!\n");
                    //所有在滑动窗口内的数据都重新发送
                    for(i = 0; i < (sender_seq_num + sender_window_size +1 - unacked_seq_num) % (sender_window_size+1); ++i)
                    {
                        tmp_sender_rtp = (rtp_header_t*)(snd_storebuffer + (unacked_seq_num + i) %(sender_window_size+1) * MAXNUM);
                        //sender_rtp->length = tmp_sender_rtp->length;
                        //printf("Sender: EOF_resent seq_num %d, length %d\n", tmp_sender_rtp->seq_num, tmp_sender_rtp->length);
                        memcpy((void*)sender_buffer, (void*)(snd_storebuffer + (unacked_seq_num + i) %(sender_window_size+1) * MAXNUM), tmp_sender_rtp->length + sizeof(rtp_header_t));
                        //sender_rtp->checksum = compute_checksum((void*)sender_buffer, tmp_sender_rtp->length + sizeof(rtp_header_t));
                        if((result = rtp_sendto(sender_sock, (void*)sender_buffer, sizeof(rtp_header_t) + tmp_sender_rtp->length, 0, (struct sockaddr*)&receiver_addr, sizeof(struct sockaddr))) < 0)
                        {
                            continue;
                        }
                    }
                    ////printf("reset timeinterval!\n");
                    timeinterval.tv_sec = 0; //计时器重置
                    timeinterval.tv_usec = 100;
                    continue;
                }
                else
                {
                    //receive results from receiver
                    if((result = recvfrom(sender_sock, sender_buffer, MAXNUM, 0, (struct sockaddr*)&receiver_addr, &addr_len)) < 0)
                    {
                        return -1;//传输出现问题
                    }
                    sender_buffer[result] = '\0';
                    sender_rtp = (rtp_header_t*)sender_buffer;
                    if((result = assert_checksum((void*)sender_buffer, sizeof(rtp_header_t))) < 0) return -1; //检查checksum
                    if(sender_rtp->type != RTP_ACK)
                    {
                        //printf("Sender: EOF_type error!\n");
                        continue;
                    }
                    else if(sender_rtp->seq_num >= unacked_seq_num + 1)
                    {
                        //printf("Sender: EOF_window size update! previous %d, now %d, expected %d\n", unacked_seq_num, sender_rtp->seq_num, sender_seq_num);
                        unacked_seq_num = sender_rtp->seq_num;
                        timeinterval.tv_sec = 0; //计时器重置
                        timeinterval.tv_usec = 100;
                    }
                }
            }
            //发送END信号
            //printf("Sender: Send END!\n");
            set_rtp_header((void*)sender_buffer, RTP_END, sender_seq_num);
            sender_rtp->checksum = compute_checksum((void*)sender_buffer, sizeof(rtp_header_t));

            if((result = rtp_sendto(sender_sock, (void*)sender_buffer, sizeof(rtp_header_t), 0, (struct sockaddr*)&receiver_addr, sizeof(struct sockaddr))) < 0)
            {
                //printf("Sender: send error!\n");
            }

            timeinterval.tv_sec = 0; //设置计时器
            timeinterval.tv_usec = 100;

            while(1) //接收END信号
            {
                //printf("Sender: waiting for end!\n");
                FD_ZERO(&set);
                FD_SET(sender_sock, &set);
                result = select(sender_sock + 1, &set, NULL, NULL, &timeinterval);
                if(result < 0) ;//printf("error!\n");
                else if(result == 0)//超时
                {
                    //printf("Sender: END_send_timeout!\n");
                    end = 1;
                    break;
                    //break;
                }
                else
                {
                    //receive ACKs from receiver
                    if((result = recvfrom(sender_sock, sender_buffer, MAXNUM, 0, (struct sockaddr*)&receiver_addr, &addr_len)) < 0)
                    {
                        //printf("Sender: END recv error!\n");
                        continue;//传输出现问题
                    }
                    sender_buffer[result] = '\0';
                    sender_rtp = (rtp_header_t*)sender_buffer;
                    //printf("Sender: receive END ACK!\n");
                    //printf("Sender: END ACK: type %d, length %d, seq_num %d\n", sender_rtp->type, sender_rtp->length, sender_rtp->seq_num);
                    if((result = assert_checksum((void*)sender_buffer, sizeof(rtp_header_t))) < 0) continue; //检查checksum

                    if(sender_rtp->type != RTP_ACK)
                    {
                        //printf("Sender: type error!\n");
                        continue;
                    }
                    else if(sender_rtp->seq_num > sender_seq_num)
                    {
                        //printf("Sender: receive seq_num: %d, sender_seq_num: %d wrong results\n", sender_rtp->seq_num, sender_seq_num);
                        continue;
                    }
                    end = 1; //此时循环终于结束
                    //printf("Sender: Receive Right END ACK, end of loope!\n");
                    break;
                    ////printf("Sender: end of one loope!\n");
                }
                
            }
        }
        c = fgetc(f);
        if(end) break;
    }

    fclose(f);
    free(databuffer);
    //free(new_message);
    return 0;
}

/**
 * @brief 用于发送数据 (优化版本的RTP)
 * @param message 要发送的文件名
 * @return -1表示发送失败，0表示发送成功
 **/
int sendMessageOpt(const char* message)
{
    //打开message
    //char* new_message = malloc(strlen(message) + 1); //warning？？？
    ////printf("%s", new_message);
    //memcpy(new_message, message, strlen(message));
    //ew_message[strlen(message)] = '\0';
    //FILE *f = fopen(new_message, "r");
    FILE *f = fopen(message, "r");
    //FILE *f = fopen("CMakeLists.txt", "r");
    //printf("%s", message);
    ////printf("%s", new_message);

    sender_seq_num = 0; //初始化seq_num
    int unacked_seq_num = 0; 
    int result; //发送或接收是否成功
    int listen_result; //接受信号
    int i; //循环指标
    int ack[sender_window_size + 1];
    for(i = 0; i < (sender_window_size + 1); ++i) ack[i] = 0;

    char* databuffer = malloc(MAXNUM);//读取数据的缓冲区
    char c; //从file中一个字节一个字节的读取
    int count = 0; //表明已经读取到了哪个位置 
    int end; //标记循环结束
    struct timeval timeinterval; //计时器
    fd_set set; 
    socklen_t addr_len = sizeof(struct sockaddr_in);
    rtp_header_t* tmp_sender_rtp;

    //printf("******\n");
    // while((c = fgetc(f)) != EOF)
    // {
    //     //printf("%c", c);
    // }
    // //printf("\n");
    //char package[]
    c = fgetc(f);
    while(1)
    {
        ////printf("%c", c);
        int end = 0;
        databuffer[count] = c;
        if(!feof(f)) count ++;

        if(count == PAYLOAD_SIZE -1 || (feof(f) && count != 0)) //将这一段打包发送出去
        {
            int fileend = feof(f);
            databuffer[count] = '\0';
            ////printf("\n");
            ////printf("count:%d, file:%s\n", count, databuffer);
            //int j = 0;
            while(1)
            {
                //printf("------------ send a new package -----------\n");
                //if(j > 10) break;
                ////printf("sender_seq_num: %d, unacked_seq_num: %d, window_size: %d\n", sender_seq_num, unacked_seq_num, sender_window_size);
                while(1)//接收过程，尝试接受一个
                {
                    if((unacked_seq_num == sender_seq_num)) 
                    {
                        //printf("empty in window!\n");
                        break; //说明滑动窗口是空的
                    }
                    FD_ZERO(&set);
                    FD_SET(sender_sock, &set);
                    listen_result = select(sender_sock + 1, &set, NULL, NULL, &timeinterval);
                    ////printf("listen_result %d, in trying to recv.\n", listen_result);
                    // if(j > 10) {end = 1;break;}
                    // j++;
                    if(listen_result < 0); //printf("error!\n"); //不知道什么情况
                    else if(listen_result == 0)//超时
                    {
                        ////printf("send_timeout! resend now!\n");
                        //所有在滑动窗口内的数据都重新发送
                        //printf("unacked %d, sender %d\n", unacked_seq_num, sender_seq_num);
                        for(i = 0; i < (sender_seq_num + sender_window_size +1 - unacked_seq_num) % (sender_window_size + 1); ++i)
                        {
                            if(ack[(unacked_seq_num + i)%(sender_window_size + 1)] == 1) 
                            {
                                //printf("%d: already sent!\n", i);
                                continue;
                            }                             
                            ////printf("resend %d!\n", i + unacked_seq_num);
                            tmp_sender_rtp = (rtp_header_t*)(snd_storebuffer + (unacked_seq_num + i) %(sender_window_size+1) * MAXNUM);
                            //sender_rtp->length = tmp_sender_rtp->length;
                            //printf("resent seq_num %d, length %d\n", tmp_sender_rtp->seq_num, tmp_sender_rtp->length);
                            memcpy((void*)sender_buffer, (void*)(snd_storebuffer + (unacked_seq_num + i) %(sender_window_size+1) * MAXNUM), tmp_sender_rtp->length + sizeof(rtp_header_t));
                            sender_rtp = (rtp_header_t*)sender_buffer;
                            //sender_rtp->checksum = compute_checksum((void*)sender_buffer, tmp_sender_rtp->length + sizeof(rtp_header_t));
                            if((result = rtp_sendto(sender_sock, (void*)sender_buffer, sizeof(rtp_header_t) + tmp_sender_rtp->length, 0, (struct sockaddr*)&receiver_addr, sizeof(struct sockaddr))) < 0)
                            {
                                continue;
                            }
                        }
                        ////printf("reset timeinterval!\n");
                        timeinterval.tv_sec = 0; //计时器重置
                        timeinterval.tv_usec = 100;
                        //continue;
                    }
                    else// 收到包了
                    {
                        //receive results from receiver
                        if((result = recvfrom(sender_sock, sender_buffer, MAXNUM, 0, (struct sockaddr*)&tmpt_receiver, &addr_len)) < 0)
                        {
                            continue;//传输出现问题
                        }
                        sender_buffer[result] = '\0';
                        sender_rtp = (rtp_header_t*)sender_buffer;
                        //printf("Receiver: receive %d\n", sender_rtp->seq_num);
                        if((result = assert_checksum((void*)sender_buffer, sizeof(rtp_header_t))) < 0) continue; //检查checksum
                        if(sender_rtp->type != RTP_ACK)
                        {
                            //printf("type error!\n");
                            continue;
                        }
                        else if(sender_rtp->seq_num >= unacked_seq_num)
                        {
                            //printf("receive new package! unacked %d, this acked %d\n", unacked_seq_num, sender_rtp->seq_num);
                            ack[sender_rtp->seq_num % (sender_window_size + 1)] = 1;
                            if(sender_rtp->seq_num == unacked_seq_num)
                            {
                                for(i = 0; i < sender_window_size; ++i)
                                {
                                    if(ack[(unacked_seq_num + i) % (sender_window_size + 1)] == 1)
                                    {
                                        ack[(unacked_seq_num + i) % (sender_window_size + 1)] = 0; //重置
                                    }
                                    else
                                        break;
                                }
                                //printf("window size updataed! previous unacked %d, new unacked %d.\n", unacked_seq_num, unacked_seq_num + i);
                                unacked_seq_num = unacked_seq_num + i; //更新unacked
                                timeinterval.tv_sec = 0; //计时器重置
                                timeinterval.tv_usec = 100;
                            }
                            ////printf("reset timeinterval!\n");
                            
                            break;//跳出尝试接受的loope
                        }
                    }
                    if(sender_seq_num < (unacked_seq_num + sender_window_size))
                    {
                        //printf("not receive, but still have sender window!\n");
                        break;
                    }
                }
                //尝试发送
                if(sender_seq_num < unacked_seq_num)//不知道什么情况，应该有问题
                {
                    sender_seq_num = unacked_seq_num;
                    //printf("unknown error occur!\n");
                    return -1;
                }
                else if(sender_seq_num < (unacked_seq_num + sender_window_size) && ack[sender_seq_num %(sender_window_size + 1)] == 0) //可以发送
                {
                    set_rtp_header((void*)sender_buffer, RTP_DATA, sender_seq_num);
                    sender_rtp = (rtp_header_t*)sender_buffer;
                    memcpy((void*)sender_buffer + sizeof(rtp_header_t), (void*)databuffer, count);
                    sender_rtp->length = count;
                    ////printf("%s\n", databuffer);
                    sender_rtp->checksum = compute_checksum((void*)sender_buffer, count + sizeof(rtp_header_t));

                    if((result = rtp_sendto(sender_sock, (void*)sender_buffer, count + sizeof(rtp_header_t), 0, (struct sockaddr*)&receiver_addr, sizeof(struct sockaddr))) < 0)
                    {
                        return -1; //传输出现问题
                    }
                    if(sender_seq_num == 167)
                    {
                        //printf("%s", databuffer);
                    }
                    
                    memcpy(snd_storebuffer + (sender_seq_num % (sender_window_size+1)) * MAXNUM, sender_buffer, count + sizeof(rtp_header_t)); //存储到滑动窗口，把头也存了
                    //printf("cpy to storebuffer.\n");
                    if(sender_seq_num == unacked_seq_num) //第一次开始滑动窗口
                    {
                        timeinterval.tv_sec = 0;
                        timeinterval.tv_usec = 100;
                        //printf("reset timeinterval!\n");
                    }
                    ++ sender_seq_num;
                    //printf("send: %d, count = %d, unacked = %d, fileend = %d\n", sender_seq_num, count, unacked_seq_num, fileend);
                    count = 0;
                    break;
                }
            }
        }
        else if(feof(f) && count == 0)//数据已经读取完毕，发送END
        {
            //printf("Let's send end!\n");
            //先确保数据传输完毕
            while(sender_seq_num - unacked_seq_num > 1)//接收过程
            {
                FD_ZERO(&set);
                FD_SET(sender_sock, &set);
                listen_result = select(sender_sock + 1, &set, NULL, NULL, &timeinterval);
                if(listen_result < 0); //printf("error!\n");
                else if(listen_result == 0)//超时
                {
                    //printf("EOF_send_timeout!\n");
                    //所有在滑动窗口内的数据都重新发送
                    for(i = 0; i < (sender_seq_num + sender_window_size +1 - unacked_seq_num) % (sender_window_size+1); ++i)
                        {
                            tmp_sender_rtp = (rtp_header_t*)(snd_storebuffer + (unacked_seq_num + i) %(sender_window_size+1) * MAXNUM);
                            //sender_rtp->length = tmp_sender_rtp->length;
                            memcpy((void*)sender_buffer, (void*)(snd_storebuffer + (unacked_seq_num + i) %(sender_window_size+1) * MAXNUM), tmp_sender_rtp->length);
                            sender_rtp->checksum = compute_checksum((void*)sender_buffer, tmp_sender_rtp->length + sizeof(rtp_header_t));
                            if((result = rtp_sendto(sender_sock, (void*)sender_buffer, sizeof(rtp_header_t) + tmp_sender_rtp->length, 0, (struct sockaddr*)&receiver_addr, sizeof(struct sockaddr))) < 0)
                            {
                                continue;
                            }
                        }
                        ////printf("reset timeinterval!\n");
                        timeinterval.tv_sec = 0; //计时器重置
                        timeinterval.tv_usec = 100;
                        continue;
                }
                else
                {
                    //receive results from receiver
                    if((result = recvfrom(sender_sock, sender_buffer, MAXNUM, 0, (struct sockaddr*)&receiver_addr, &addr_len)) < 0)
                    {
                        return -1;//传输出现问题
                    }
                    sender_buffer[result] = '\0';
                    sender_rtp = (rtp_header_t*)sender_buffer;
                    if((result = assert_checksum((void*)sender_buffer, sizeof(rtp_header_t))) < 0) return -1; //检查checksum
                    if(sender_rtp->type != RTP_ACK)
                    {
                        //printf("type error!\n");
                        continue;
                    }
                    else if(sender_rtp->seq_num >= unacked_seq_num + 1)
                    {
                        //printf("window size update!\n");
                        unacked_seq_num = sender_rtp->seq_num;
                        timeinterval.tv_sec = 0; //计时器重置
                        timeinterval.tv_usec = 100;
                    }
                }
            }
            //发送END信号
            set_rtp_header((void*)sender_buffer, RTP_END, sender_seq_num);
            sender_rtp->checksum = compute_checksum((void*)sender_buffer, sizeof(rtp_header_t));

            if((result = rtp_sendto(sender_sock, (void*)sender_buffer, sizeof(rtp_header_t), 0, (struct sockaddr*)&receiver_addr, sizeof(struct sockaddr))) < 0)
            {
                //printf("send error!\n");
                return -1;
            }

            timeinterval.tv_sec = 0; //设置计时器
            timeinterval.tv_usec = 100;

            while(1) //接收END信号
            {
                FD_ZERO(&set);
                FD_SET(sender_sock, &set);
                result = select(sender_sock + 1, &set, NULL, NULL, &timeinterval);
                if(result < 0); //printf("error!\n");
                else if(result == 0)//超时
                {
                    //printf("END_send_timeout!\n");
                    end = 1;
                    //break;
                }
                else
                {
                    //receive ACKs from receiver
                    if((result = recvfrom(sender_sock, sender_buffer, MAXNUM, 0, (struct sockaddr*)&receiver_addr, &addr_len)) < 0)
                    {
                        continue;//传输出现问题
                    }
                    sender_buffer[result] = '\0';
                    sender_rtp = (rtp_header_t*)sender_buffer;
                    if((result = assert_checksum((void*)sender_buffer, sizeof(rtp_header_t))) < 0) continue; //检查checksum

                    if(sender_rtp->type != RTP_ACK)
                    {
                        //printf("type error!\n");
                        continue;
                    }
                    else if(sender_rtp->seq_num > sender_seq_num)
                    {
                        //printf("receive seq_num: %d, sender_seq_num: %d wrong results\n", sender_rtp->seq_num, sender_seq_num);
                        continue;
                    }
                }
                end = 1; //此时循环终于结束
                break;
            }
        }
        c = fgetc(f);
        if(end) break;
    }

    fclose(f);
    free(databuffer);
    //free(new_message);
    return 0;

}


/**
 * @brief 用于断开RTP连接以及关闭UDP socket
 **/
void terminateSender()
{
    //while(1);
    close(sender_sock);
}
