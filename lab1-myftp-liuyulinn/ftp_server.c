#include<sys/stat.h>
#include<stddef.h>
#include<pthread.h>

#define MAGIC_NUMBER_LENGTH 6
#define MAXSIZE 1024

struct command{
    char m_protocol[MAGIC_NUMBER_LENGTH];
    char m_type;
    char m_status;
    uint32_t m_length;
} __attribute__ ((packed));


int server_recv_cmd(int sock_ctl, struct command* cmd);
int server_send_response(int sock_ctl, struct command* cmd);
int server_send_data(int sock_ctl, char* file);
int server_login(int sock_ctl, struct command* cmd);
int server_list(int sock_ctl, struct command *cmd);
int server_get(int sock_ctl, struct command *cmd);
int server_push(int sock_ctl, struct command *cmd);
void server_process(int sock_ctl);
void* handler_msg(void *arg);


int server_recv_cmd(int sock_ctl, struct command* cmd)
{
	char buffer[sizeof(cmd)];
    memset(cmd, 0, sizeof(cmd));

	if(recv_data(sock_ctl, buffer, sizeof(cmd)) == -1) return -1;
	strncpy(cmd->m_protocol, buffer, MAGIC_NUMBER_LENGTH);
	buffer[MAGIC_NUMBER_LENGTH] = cmd->m_type;
	buffer[MAGIC_NUMBER_LENGTH+1] = cmd->m_status;
	strncpy( cmd->m_length, buffer+MAGIC_NUMBER_LENGTH+3, 4);
	return 0;
}

int send_response(int sock_ctl, struct command* cmd)
{
	char buffer[sizeof(cmd)];
	strncpy(buffer, cmd->m_protocol, MAGIC_NUMBER_LENGTH);
	buffer[MAGIC_NUMBER_LENGTH] = cmd->m_type;
	buffer[MAGIC_NUMBER_LENGTH+1] = cmd->m_status;
	strncpy(buffer+MAGIC_NUMBER_LENGTH+3, cmd->m_length, 4);

	send(sock_ctl, buffer, sizeof(cmd), 0);
	return 0;
}

int server_login(int sock_ctl, struct command* cmd)
{
	char buffer[MAXSIZE];
	char user[MAXSIZE];
	char password[MAXSIZE];

	recv_data(sock_ctl, buffer, cmd->m_length - 12);

	int i = 0; int j = 0;
	while(buffer[i] != ' ') user[j++] = buffer[i++];
	user[j++] = '\0';

	j = 0;
	while(buffer[i] != '\0') password[j++] = buffer[i++];
	password[j++] = '\0';

	if((strcmp(user, "user") != 0) || strcmp(password, "123123") != 0) return -1;
	else return 1;
}


int server_list(int sock_ctl, struct command *cmd)
{
    int status;
    status = system("ls -l ftp > temp.txt");
    if(status < 0) return -1;

    int fd = open("temp.txt", O_RDONLY);

    struct stat st;
    stat("temp.txt", &st);
    size_t size = st.st_size;
	
	cmd->m_type = 0xA6;
	cmd->m_length = 12 + size;

	server_send_response(sock_ctl, cmd); //发送报文
	sendfile(sock_ctl, fd, NULL, size); //发送数据
    close(fd);

    return 0;
}

int server_get(int sock_ctl, struct command * cmd)
{
	char filename[MAXSIZE];
	recv_data(sock_ctl, filename, cmd->m_length - 12); //读取file name

	int fd=open(filename,O_RDONLY);
	if(fd<0)
	{
		cmd->m_type = 0xA8;
		cmd->m_status = 0;
		cmd->m_length = 12;
		send_response(sock_ctl, cmd);        //发送错误码
		return -1;
	}
	else
	{
		cmd->m_type = 0xA8;
		cmd->m_status = 1;
		cmd->m_length = 12;
		server_send_response(sock_ctl, cmd);        //发送 ok

		struct stat st;
		stat(filename, &st);
		size_t size=st.st_size;

		cmd->m_type = 0xFF;
		cmd->m_length = 12 + size;
		send_response(sock_ctl, cmd);     //发送文件传输表头
		sendfile(sock_ctl, fd, NULL, size);     //发送文件
		close(fd);
		return 0;
	}
}

int server_push(int sock_ctl, struct command* cmd)
{
	char filename[MAXSIZE];
	recv_data(sock_ctl, filename, cmd->m_length - 12);  //接受filename

	int fd=open(filename,O_CREAT|O_WRONLY,0664);
	char data[MAXSIZE];
	memset(data,0,sizeof(data));
	server_recv_cmd(sock_ctl, cmd); //接受data的表头信息
	ssize_t s=recv(sock_ctl, data, cmd->m_length - 12, 0); //接受data
	write(fd,data,s);
	close(fd);
	return 0;
}

void server_process(int sock_ctl)
{
	struct command* cmd;
	server_recv_cmd(sock_ctl, cmd); //接受信号
	cmd->m_type = 0xA2;
	send_response(sock_ctl, cmd); //返回open_conn_reply

	server_recv_cmd(sock_ctl, cmd); //接受login
	if(server_login(sock_ctl, cmd) == 1) //返回auth_reply
	{
		cmd->m_type = 0xA4;
		cmd->m_status = 1;
		cmd->m_length = 12;
		send_response(sock_ctl, cmd);
	}
	else
	{	
		cmd->m_type = 0xA4;
		cmd->m_status = 0;
		cmd->m_length = 12;
		send_response(sock_ctl, cmd);
		return;
	}


	while(1) //接受处理LOOPE
	{
		server_recv_cmd(sock_ctl, cmd);
		if(cmd->m_type == 0xAB) //quit
		{
			cmd->m_type = 0xAC;
			send_response(sock_ctl, cmd);
			break;
		}

		if(cmd->m_type == 0xA5) server_list(sock_ctl, cmd); //ls
		else if(cmd->m_type == 0xA7) server_get(sock_ctl, cmd); //get
		else if(cmd->m_type == 0xA9) server_push(sock_ctl, cmd); //put
	}
}

void* handler_msg(void *arg)
{
	pthread_detach(pthread_self());
	int sock = (int)arg;
	server_process(sock);
	close(sock);
	return NULL;
}

int main(char ** argv) {

	int sock = socket_create(argv[0], atoi(argv[1]));

	while(1)
	{
		int sock_ctl = socket_accept(sock);
		if(sock_ctl < 0) continue;
		pthread_t tid;
		pthread_create(&tid, NULL, handler_msg, (void*) sock_ctl);
	}
	close(sock);
	return 0;
}