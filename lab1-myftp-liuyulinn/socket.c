#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<arpa/inet.h>
#include<netinet/in.h>
#include<sys/socket.h>
#include<sys/types.h>
#include<sys/sendfile.h>
#include<unistd.h>
#include<ctype.h>
#include<fcntl.h>
#include<errno.h>

#define MAXSIZE 1024
#define CLIENT_PORT 3306

int socket_create(const char* ip, const int port);
int socket_accept(int sock);
int socket_connect(const char* ip, const int port);
int recv_data(int sock, char* buffer, int buffersize);


int socket_create(const char* ip, const int port)
{
    if((ip == NULL) || (port < 0)) return -1;
    int sock = socket(AF_INET, SOCK_STREAM, 0); //创建TCP套接字
    if(sock < 0) return -1;
 
    int op = 1;
    setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &op, sizeof(op)); //设置收发限时

    struct sockaddr_in local;
    local.sin_family = AF_INET;
    local.sin_port = htons(port);
    local.sin_addr.s_addr = inet_addr(ip);

    if(bind(sock, (struct sockaddr*)&local, sizeof(local)) < 0) 
    //将socket和这个ip+端口绑定，这样流经该ip地址和端口的数据才会交给套接字处理
    {
        return -1;
    }

    if(listen(sock, 5) < 0) //进入监听状态
    {
        return -1;
    }

    return sock;
}
int socket_accept(int sock)
{
    struct sockaddr_in client;
    socklen_t len = sizeof(client);

    int sock_new = accept(sock, (struct sockaddr*)&client, &len);
    //返回一个新的套接字来和客户端通信，local保存了客户端的ip和端口号
    //之后与客户端通信，使用这个新生成的套接字
    if(sock_new < 0) return -1;
    return sock_new;
}

int socket_connect(const char* ip, const int port)
{
    if((ip == NULL) || (port < 0)) return -1;
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if(sock < 0) return -1;

    struct sockaddr_in server;
    server.sin_family = AF_INET;
    server.sin_port = htons(port);
    server.sin_addr.s_addr = inet_addr(ip);

    if(connect(sock, (struct sockaddr*)&server, sizeof(server)) < 0) return -1;
    //将sock和与服务器通信的端口绑定
    return sock;
}

int recv_data(int sock, char* buffer, int buffersize)
{
    memset(buffer, 0, buffersize);
    ssize_t s =recv(sock, buffer, buffersize, 0);
    //ssize_t: signed int
    if(s <= 0) return -1;
    return s;
}


