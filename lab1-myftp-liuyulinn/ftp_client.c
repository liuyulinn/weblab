#define MAGIC_NUMBER_LENGTH 6
#define MAXSIZE 1024

#include<string.h>
#include<stdlib.h>
#include<stddef.h>


struct command{
    char m_protocol[MAGIC_NUMBER_LENGTH];
    char m_type;
    char m_status;
    uint32_t m_length;
} __attribute__ ((packed));

int client_read_command(char* buffer, int size, strcut command *cmd); // 获取客户端命令
int client_send_command(int sock_ctl, struct command *cmd); //向服务器发送命令
int read_reply(int sock_ctl); //获得服务器回复

int client_login(int sock_ctl); //登录

int client_list(int sock_data, int sock_ctl); //list命令
int client_get(int sock_data, int sock_ctl, char *filename); //get命令
int client_push(int sock_data, int sock_ctl, char *filename); //push命令



int client_read_command(char* buffer, int size, strcut command *cmd, char *arg)
{
    memset(cmd->m_protocol, 0, sieof(cmd->m_protocol));
    memset(cmd->m_type, 0, sizeof(cmd->m_type));
    memset(cmd->m_status, 0, sizeof(cmd->m_status));
    memset(cmd->m_length, 0, sizeof(cmd->m_length));

    printf("Client> ");

    fflush(stdout);
    read_input(buffer, size);

    arg = strtok(buffer, " ");
    arg = strtok(NULL, " ");

    if(strcmp(buffer, "open") == 0)
    {
        cmd->m_protocol = 0xe3myftp;
        cmd->m_type = 0xA1;
        cmd->m_length = 13;
    }
    else if(strcmp(buffer, "auth") == 0)
    {
        cmd->m_protocol = 0xe3myftp;
        cmd->m_type = 0xA3;
        cmd->m_length = 13;
    }
    else if(strcmp(buffer, "ls") == 0)
    {
        cmd->m_protocol = 0xe3myftp;
        cmd->m_type=0xA5;
        cmd->m_length = 12;
    }
    else if(strcmp(buffer, "get") == 0)
    {
        cmd->m_protocol = 0xe3myftp;
        cmd->m_type = 0xA7;
        cmd->m_length = 13;
    }
    else if(strcmp(buffer, "put") == 0)
    {
        cmd->m_protocol = 0xe3myftp;
        cmd->m_type = 0xA9;
        cmd->m_length = 13;
    }
    else if(strcmp(buffer, "quit") == 0)
    {
        cmd->m_protocol = 0xe3myftp;
        cmd->m_type = 0xAB;
        cmd->m_length = 12;
    }
    else return -1;
}

int client_send_command(int sock_ctl, struct command *cmd)
{
    char buffer[MAXSIZE];
    sprintf(buffer, "%s %s %s %s", cmd->m_protocol, cmd->m_type, cmd->m_status, cmd->m_length);
    if(send(sock_ctl, buffer, strlen(buffer), 0) < 0)
    {
        printf("error sending command to server.\n");
        return -1;
    }
    return 0;
}

int client_send_data(int sock_ctl, char* arg)
{
    char buffer[MAXSIZE];
    sprintf(buffer, "%s", arg);
    if(send(sock_ctl, buffer, strlen(buffer), 0) < 0)
    {
        printf("error sending command to server.\n");
        return -1;
    }
    return 0;
}

struct command read_reply(int sock_ctl)
{
    struct command cmd;
    if(recv(sock_ctl, &cmd, sizeof(cmd), 0) < 0)
    {
        printf('client: error.\n');
        return -1;
    }
    return cmd;
}

int client_connect(struct command *cmd, char *arg)
{
    sock_connect(arg[0], atoi(arg[1]));
    send(sock_ctl, cmd, cmd->m_length);
    cmd = read_reply(sock_ctl);
    if(cmd->m_type != 0xA2)
    {
        printf("Error: not connect to server.\n");
        return -1;
    }
    else
    {
        printf("connected to server now.\n");
        return 0;
    }
}

int client_login(int sock_ctl, struct command *cmd, char *arg)
{
    uint32_t arg_length = strlen(arg);
    cmd->m_length += arg_length;
    
    client_send_command(sock_ctl, &cmd);
    client_send_data(sock_ctl, &arg);

    cmd = read_reply(sock_ctl);
    if(cmd->m_type == 0xA4)
    {
        if(cmd->m_status)
        {
            printf("Successful login.\n"); return 0;
        }
        else
        {
            printf("Invalid username/password.\n"); return -1;
        }
    }
    else
    {
        printf("unknown error.\n"); return 0;
    }
}


int client_list(int sock_ctl, struct command *cmd)
{
    uint32_t list_length;
    char buffer[MAXSIZE];

    client_send_command(sock_ctl, &cmd);
    cmd = read_reply(sock_ctl);
    if(cmd->m_type == 0xA6)
    {
        list_length = cmd->m_length - 13;
    }
    else
    {
        printf("can't get client list.\n"); return -1;
    }

    memset(buffer, 0, sizeof(buffer));
    ssize_t s=recv(sock_data,buffer,list_length,0);
    if(s<=0)
    {
        if(s<0)
            perror("error");
        break;
    }
    buffer[list_length + 1] = '\0';
    printf("%s",buffer);
}

int client_get(int sock_ctl, struct command *cmd, char *filename)
{
    uint32_t list_length;
    client_send_command(sock_ctl, &cmd);
    cmd = read_reply(sock_ctl);
    if(cmd->m_status)
    {
        list_length = cmd->m_length - 13;
        int fd=open(filename, O_CREAT|O_WRONLY, 06694);
        
        ssize_t s = 0;
        char data[MAXSIZE];
        memset(data, 0, sizeof(data));
        s = recv(sock_data, data, sizeof(data), 0)
        if(s < 0){
            perror("error"); break;
        }
        if(s == 0) break;
        write(fd, data, s);
        close(fd);
        return 0;
    }
    else
    {
        printf("can't get file from server.\n");
        return -1;
    }
 
}

int client_push(int sock_data, struct command *cmd, char *filename)
{
    int fd=open(filename, O_RDONLY);
    if(fd < 0) return -1;
    
    struct stat st;
    if(stat(filename, &st) < 0) return -1;

    cmd->m_length += st.st_size;
    client_send_command(sock_ctl, &cmd);
    client_send_data(sock_ctl, &arg);
    cmd = read_reply(sock_ctl);

    if(cmd->m_type == 0xAA)
    {
        client_send_data(sock_ctl, fd, st.st_size);
        close(fd);
        return 0;
    }
    else
    {
        printf("can't push file to server.\n");
        return -1;
    }
}




int main() {

    char* buffer;
    char* arg;
    struct command* cmd;

    int sock_ctl, sock_data;

    //连接到服务器循环
    while(1)
    {
        if(client_read_command(buffer, sizeof(buffer), &cmd, &arg) <0) // 获取用户指令
        {
            printf("Invalid command.\n");
            continue;
        }
        if(strcmp(buffer, "open") == 0) //连接
        {
            sock_ctl = client_connect(cmd, arg);
            if(sock_ctl)
            {
                printf("Connected to the server now.\n");
                break;
            }
            else
            {
                printf("cannot connet to the server.\n");
            }
        }
        else
        {
            printf("Please connect first.\n"); //请先建立连接
        }
    }

    //身份认证循环
    while(1)
    {
        if(client_read_command(buffer, sizeof(buffer), &cmd, &arg) <0) // 获取用户指令
        {
            printf("Invalid command.\n");
            continue;
        }
        if(strcmp(buffer, "auth") == 0) //身份认证
        {
            if(client_login(sock_ctl, arg))
            {
                printf("Authentification passes.\n");
                auth = 1;
            }
            else
            {
                printf("user or password invaild.\n");
            }
        }
        else
        {
            printf("Please authentication first.\n"); //请先进行身份认证
        }
    }

    //客户机读取命令循环
    while(1){

        if(client_read_command(buffer, sizeof(buffer), &cmd, &arg) <0) // 获取用户指令
        {
            printf("Invalid command.\n");
            continue;
        }

        sock_data = client_open_data_conn(sock_ctl); //创建数据连接
        if(sock_data < 0) //数据连接失败
        {
            print("ERROR");
            break;
        }

        if(strcmp(buffer, "ls") == 0) client_list(sock_ctl, sock_data);
        else if(strcmp(buffer, "get") == 0) client_get(sock_ctl, sock_data, arg);
        else if(strcmp(buffer, "psuh") == 0) client_push(sock_ctl, sock_data, arg);
        close(sock_data);

        if(strcmp(buffer, "quit") == 0) break;
    }
    close(sock_ctl);
    printf("Thank you.");
    return 0;
}