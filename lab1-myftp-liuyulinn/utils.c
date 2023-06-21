void read_input(char* buffer, int buffersize);
void trimstr(char* str, int n);

void read_input(char* buffer, int buffersize)
//从输入中读取一行
{
    memset(buffer, 0, buffersize);
    if(fget(buffer, buffersize, stdin) != NULL)
    {
        char *n = strchr(buffer, '\n');
        if(n) *n = '\0';
    }
}
void trimstr(char* str, int n)
//去掉字符串中的空白和换行
{
    int i = 0;
    for(i = 0; i < n; ++ i)
    {
        if(isspace(str[i]) || (str[i] == '\0')) str[i] = 0;
    }
}