#include "router.h"
#define Infinity 100
#define MAX 100

RouterBase* create_router_object() {
    return new Router;
}
/*新建一个router，router中含有:
port_num: 最大的端口数
external_port: 记录externa_port
cost: 记录到各个port的距离
DV: DV向量表，包含（ip，cost，next_hop) 用一个vector实现
NAT: 实现NAT功能的数据结构
    包含：available_addr, 记录available addr的基
        num_addr, 包含多少个available addr
        ip, 记录分配了的ip addr
        allocated_addr, 已经分配了多少个ip*/

void Router::router_init(int port_num, int external_port, char* external_addr, char* available_addr) {
    Router* newRouter;
    portNum = port_num;
    if(external_port != 0) //如果external_port不为零，记录external_port的性质，并且将external_addr加入到DV向量表格中，并记录available addr
    {
        externalPort = external_port;
        for(int i = 0; i < portNum; ++i) cost[i] = -1; //表示关闭
        //将external_addr加入DV向量表格
        Node extern_ip; 
        int networkNum;
        uint32_t external_addr_uint = get_ip_net(external_addr, &networkNum);
        extern_ip.ip = external_addr_uint;
        extern_ip.next_hop = external_port;
        extern_ip.cost = 0;
        extern_ip.range = 1;
        for(int i = 0; i < (32 - networkNum); ++i)
            extern_ip.range = 2*extern_ip.range;
        DV.push_back(extern_ip);

        //将available addr记录到available addr里
        uint32_t available_ip = get_ip_net(available_addr, &networkNum);
        //printf("networkNum:%d", networkNum);
        nat.availableIp = available_ip;
        nat.addrNum = 1;
        for(int i = 0; i < (32 - networkNum); ++i)
            nat.addrNum = 2*nat.addrNum;
        for(int i =0; i < nat.addrNum; ++i) nat.allocatedIp[i] = 0;
        nat.allocatedIpNum = 0;
    }
    else
    {
        externalPort = 0;
        for(int i = 0; i < portNum; ++i) cost[i] = -1; //表示关闭
    }
    // printf("successfully create!\n");
    // printf("external Port: %u\n",externalPort);
    // for(int i = 0; i < DV.size(); ++i)
    // {
    //     printNode(DV[i]);
    // }
    // for(int i = 0; i < portNum; ++i)
    // {
    //     printf("port:%d, cost: %d\n", i+1, cost[i]);
    // }
    // printf("nat: addrNum: %d\n", nat.addrNum);
    // printf("nat: availableIp: %X\n", nat.availableIp);
    return;
}

// package转发功能
// 0, 广播，-1，丢弃，1，转发到默认网关
/*根据packet报文，分3类实现：
    TYPE_DV: 交换路由器之间距离向量
        payload为另一个端口的DV表
    TYPE_DATA: 实现数据转发
        根据dst查询路由表，根据查找到的端口转发
        如果转发的端口是外网，则需要实现nat功能
    TYPE_CONTROL:
*/
int Router::router(int in_port, char* packet) {
    FILE* f=fopen("test.txt", "a");
    //fprintf(f,"******read in_port : %d******\n", in_port);
    //printf("print present cost and DV\n");
    // for(int i = 0; i < DV.size(); ++i)
    // {
    //     printNode(DV[i]);
    // }
    // for(int i = 0; i < portNum; ++i)
    // {
    //     printf("port:%d, cost: %d\n", i+1, cost[i]);
    // }

    router_head rhead = (router_head) packet;
    //printf("router_head: length %d\n", rhead->length);
    //if(rhead->length > 20) return -1;
    if(rhead->type == TYPE_DV) //TYPE_DV，路由器之间交换距离向量
    {
        //fprintf(f,"update DV!\n");
        //printf("exchange DV vector!\n");
        int changeFlag = -1; //如果本地dv表格有更改，则修改changeFlag为0，泛洪传播，否则丢弃报文
        //将packet + sizeof(router_head)读取为一个(ip, cost, next_hop)的表格
        int nodeNum = rhead->length / sizeof(Node);
        //printf("----total nodeNum %d-----\n", nodeNum);
        pnode pneighborNode;
        int count = 0; //记录要传播多少条数据
        for(int i = 0; i < nodeNum; ++i)
        {
            pneighborNode = (pnode)(packet + sizeof(head) + i * sizeof(Node));
            // printf("read neighborNode %d ", i);
            // printNode(*pneighborNode);
            //如果记录neighborNode ip在本地表格中，比较两个的大小
            int tmp;
            if((tmp = isContain(pneighborNode->ip)) >= 0)
            {
                if(DV[tmp].cost != 0)// 否则要么是host，要么是external port
                {
                    int addCost = max(pneighborNode->cost, 0) + cost[in_port -1];
                    // if(DV[tmp].next_hop == in_port && cost[in_port - 1] != -1)
                    // {
                    //     DV[tmp].cost = Infinity;
                    //     DV[tmp].next_hop = in_port;
                    //     changeFlag = 0;
                    //     memcpy((void*)(packet + sizeof(head) + count * sizeof(Node)), (void*)&DV[tmp], sizeof(Node));
                    //     count ++;
                    //     DV[tmp].cost = addCost;
                    //     fprintf(f, "update DV: tmp %d inport %d next_hop %d cost %d\n", tmp, in_port, DV[tmp].next_hop, DV[tmp].cost);
                    // }
                    if(DV[tmp].cost > addCost)//|| DV[tmp].next_hop == in_port)
                    {
                        DV[tmp].cost = addCost;
                        DV[tmp].next_hop = in_port;
                        //fprintf(f, "update DV: tmp %d inport %d next_hop %d cost %d\n", tmp, in_port, DV[tmp].cost, DV[tmp].next_hop);
                        changeFlag = 0;
                        memcpy((void*)(packet + sizeof(head) + count * sizeof(Node)), (void*)&DV[tmp], sizeof(Node));
                        count ++;
                    }
                }
            }
            //如果记录不在本地表格中，像本地表格添加neighborNode
            else
            {   //一定不会是host或者external port，所以不需要做判断
                pneighborNode->cost = max(pneighborNode->cost, 0) + cost[in_port -1];
                pneighborNode->next_hop = in_port;
                //fprintf(f, "update DV: tmp %d next_hop %d cost %d \n", tmp, pneighborNode->next_hop, pneighborNode->cost);
                DV.push_back(*pneighborNode);
                changeFlag = 0;
                memcpy((void*)(packet + sizeof(head) + count * sizeof(Node)), (void*)pneighborNode, sizeof(Node));
                count ++;
            }
        }
        if(changeFlag == 0) //说明存在修改，需要泛洪广播，所以修改packet，将本地的DV表格转发
        {
            // for(int i = 0; i < DV.size(); ++ i)
            // {
            //     memcpy((void*)(packet + sizeof(head) + i * sizeof(Node)), (void*)&DV[i], sizeof(Node));
            //     pnode debug;
            //     debug = (pnode)(packet + sizeof(head) + i * sizeof(Node));
            //     printf("--debug--send vector\n");
            //     printNode(*debug);
            // }
            rhead->length = sizeof(Node) * count;
            // printf("print updated cost and DV\n");
            // for(int i = 0; i < DV.size(); ++i)
            // {
            //     printNode(DV[i]);
            // }
            // fclose(f);
            return 0;
        }
        // printf("No changeFlag\n");
        // fclose(f);
        return -1;
    }
    else if(rhead->type == TYPE_DATA) //TYPE_DATA，实现数据转发功能
    {
        //fprintf(f,"export data!\n");
        int result;
        if(in_port == externalPort) //如果是从external port中接受packet
        {
            // fprintf(f,"availableIp %X\n", nat.availableIp);
            // fprintf(f,"allocated Ip %d\n", nat.allocatedIpNum);
            // for(int i =0; i < nat.addrNum; ++i)
            // {
            //     if(nat.allocatedIp[i] != 0) printf("%d %X\n", i, nat.allocatedIp[i]);
            // }
            //fprintf(f,"src: %X, dst: %X\n", htonl(rhead->src), htonl(rhead->dst));
            if((result = nat.inport(rhead)) == 1) 
            {
                //fclose(f);
                return -1; //不明发送目的地
            }
            //fprintf(f,"modified dst: %X\n", htonl(rhead->dst));
        }
        uint32_t dst = htonl(rhead->dst);
        //查询路由表
        //fprintf(f,"dst: %X\n", dst);
        int tmp = isContain(dst);
        // for(int i = 0; i < DV.size(); ++i)
        // {
        //     fprintf(f,"DV-%d ",i); fprintf(f,"Ip: %X, cost: %d, next_hop: %d\n", DV[i].ip, DV[i].cost, DV[i].next_hop);
        // }
        //fprintf(f,"send data DV %d, inport %d, outport %d, cost %d, cost_port %d\n", tmp, in_port, DV[tmp].next_hop, DV[tmp].cost, cost[DV[tmp].next_hop - 1]);
        if(tmp == -1)
        {
            return 1; //说明不在路由表内
            //fclose(f);
        }
        int out_port = DV[tmp].next_hop;
        //fprintf(f,"out_port: %d\n", out_port);
        if(out_port == externalPort) //如果要发送到external port
        {
            // fprintf(f,"availableIp %X\n", nat.availableIp);
            // fprintf(f,"allocated Ip %d\n", nat.allocatedIpNum);
            // for(int i =0; i < nat.addrNum; ++i)
            // {
            //     if(nat.allocatedIp[i] != 0) fprintf(f,"%d %X\n", i, nat.allocatedIp[i]);
            // }
            // fprintf(f,"src: %X, dst: %X\n", htonl(rhead->src), htonl(rhead->dst));
            if((result = nat.outport(rhead)) == -1)
            {
                //fclose(f);
                return -1; //说明应该丢弃报文
            }
            //fprintf(f,"modified src: %X\n", htonl(rhead->src));
        }
        //fclose(f);
        return out_port;
    }
    else if(rhead->type == TYPE_CONTROL) //TYPE_CONTROL
    {
        //printf("receive control information!\n");
        char* payload = packet + sizeof(head);
        if((*payload - '0') == 0) //Trigger DV send
        {
            //printf("trigger!\n");
            for(int i = 0; i < DV.size(); ++ i)
            {
                memcpy((void*)(packet + sizeof(head) + i * sizeof(Node)), (void*)&DV[i], sizeof(Node));
                pnode debug;
                debug = (pnode)(packet + sizeof(head) + i * sizeof(Node));
                // printf("--debug--send vector\n");
                // printNode(*debug);
            }
            rhead->length = sizeof(Node) * DV.size();
            rhead->type = TYPE_DV;
            return 0;
        }
        else if((*payload - '0') == 1)
        {
            //printf("release ip in NAT!\n");
            payload[rhead->length] = '\0';
            uint32_t internal_ip = get_ip(payload + 2);
            //fprintf(f, "interal_ip\n: %X", internal_ip);
            nat.release(internal_ip);
            return -1;
        }
        else if((*payload - '0') == 2)
        {
            //printf("change network topology!\n");
            int changeFlag = -1;
            payload[rhead->length] = '\0';
            //printf("read string: %s\n", payload);
            char buffer[10];
            int i;
            for(i = 2; i < rhead->length; ++i)
            {
                if(payload[i] != ' ') buffer[i-2] = payload[i];
                else
                {
                    buffer[i] = '\0';
                    break;
                }
            }
            int port = atoi(buffer);
            //printf("get port: %d", port);
            memcpy(buffer, payload + i, rhead->length - i);
            buffer[rhead->length - i] = '\0';
            int value = atoi(buffer);
            //printf("get value: %d\n", value);

            //fprintf(f, "change value %d\n", value);
            //if(value == -1) fprintf(f, "********Close that port!*****************\n");
            if(cost[port -1] == -1 && value == -1) return -1;

            int count = 0;
            for(int i = 0; i < DV.size(); ++ i)
            {
                if(DV[i].next_hop == port)
                {
                    if(value == -1) //说明该端口被关闭
                    {
                        DV[i].cost = Infinity;
                    }
                    else
                    {
                        DV[i].cost = DV[i].cost + value - max(cost[port - 1], 0);
                    }
                    memcpy((void*)(packet + sizeof(head) + count * sizeof(Node)), (void*)&DV[i], sizeof(Node));
                    count ++;
                    changeFlag = 0;
                }
            }
            cost[port - 1] = value;
            if(changeFlag == 0) //说明存在修改，需要泛洪广播，所以修改packet，将本地的DV表格转发
            {
                rhead->length = sizeof(Node) * count;
                rhead->type = TYPE_DV;
            }
            // printf("print updated cost and DV\n");
            // for(int i = 0; i < DV.size(); ++i)
            // {
            //     printNode(DV[i]);
            // }
            // for(int i = 0; i < portNum; ++i)
            // {
            //     printf("port:%d, cost: %d\n", i+1, cost[i]);
            // }
            return changeFlag;
        }
        else if((*payload - '0') == 3) //add host
        {
            //printf("add host!\n");
            //return -1;
            payload[rhead->length] = '\0';
            int i;
            char buffer[10];
            for(i = 2; i < rhead->length; ++i)
            {
                if(payload[i] != ' ') buffer[i - 2] = payload[i];
                else
                {
                    buffer[i - 2] = '\0';
                    break;
                }
            }
            int port = atoi(buffer);
            //printf("get port: %d\n", port);
            
            uint32_t internal_ip = get_ip(payload + i);
            //printf("%s\n", payload + i);
            //printf("get ip %X\n", internal_ip);
            //printf("what happen?");

            Node extern_ip; 
            extern_ip.ip = internal_ip;
            extern_ip.next_hop = port;
            extern_ip.cost = 0;
            extern_ip.range = 1;
            //printf("2");
            DV.push_back(extern_ip);
            //printf("3");
            for(i = 0; i < DV.size(); ++i) printNode(DV[i]);
            //发送消息给其他路由器，只发送变化了的消息
            memcpy((void*)(packet + sizeof(head)), (void*)&extern_ip, sizeof(Node));
            rhead->length = sizeof(Node);
            rhead->type = TYPE_DV;
            //printf("successful send\n");
            return 0;
        }
        else
        {
            printf("not implemented!\n");
        }
    }
    else
    {
        printf("wrong type!");
        return -1;
    }
    return -1; //出现了不明错误
}

int Router::isContain(uint32_t myip)
{
    for(int i = 0; i < DV.size(); ++i)
    {
        if((myip - DV[i].ip) < DV[i].range) return i;
    }
    return -1;
}

/*return 0: 成功转换； 1: 不明ip地址（没有记录在案的地址），发送到controller*/
int NAT::inport(router_head rhead)
{
    uint32_t dst = htonl(rhead->dst);
    int i = dst - availableIp;
    if(i >= 0 && i < addrNum)
    {
        if(allocatedIp[i] != 0)
        {
            rhead->dst = htonl(allocatedIp[i]);
            return 0;
        }
        else //不明ip地址
            return 1; 
    }
    return 1; //不明ip地址
}

/*return 0: 成功转换，-1: 没有可用的公网地址，丢弃包*/
int NAT::outport(router_head rhead)
{
    uint32_t src = htonl(rhead->src);
    for(int i = 0; i < addrNum; ++i)
    {
        if(allocatedIp[i] == src)
        {
            rhead->src = htonl(availableIp + i);
            return 0;
        }
    }
    printf("Not found allocated Ip!\n");
    //没有成功找到
    if(allocatedIpNum == addrNum) return -1; //没有空闲的可用公网地址
    for(int i = 0; i < addrNum; ++i)
    {
        if(allocatedIp[i] == 0) //空闲的公网地址
        {
            printf("allocate i for Ip %X\n", availableIp + i);
            rhead->src = htonl(availableIp + i);
            printf("modified src should be: %X\n", rhead->src);
            allocatedIp[i] = src;
            allocatedIpNum ++;
            return 0;
        }
    }
    return -1; //出现了不明错误
}

/*将ip从allocatedIp表中移出*/
void NAT::release(uint32_t ip)
{
    for(int i = 0; i < addrNum; ++i)
    {
        if(allocatedIp[i] == ip)
        {
            allocatedIp[i] = 0;
            allocatedIpNum --;
            return;
        }
    }
    return;
}

uint32_t get_ip_net(char* ip, int* num)
{
    uint32_t ip_addr = 0;
    uint32_t mask = 0;
    string append;

    for(int i =0; i < strlen(ip); ++i)
    {
        if(ip[i] == '.' || ip[i] == '/' || i == strlen(ip) - 1)
        {
            if(i == strlen(ip) - 1)
            {
                append += ip[i];
            }
            if(i != (strlen(ip) - 1))
            {
                mask = (unsigned char)atoi(append.c_str());
                ip_addr = ip_addr | mask; 
            }
            if(i != strlen(ip) -1 && ip[i] != '/')
                ip_addr = ip_addr << 8;
            if(i == strlen(ip) - 1)
            {
                //printf("got string %s\n", append.c_str());
                int number = atoi(append.c_str());
                //printf("got number%d\n", number);
                *num = number;
            }
            // printf("%s\n", append.c_str());
            // printf("%X\n", ip_addr);
            append = "";
        }
        else
        {
            append += ip[i];
        }
    }
    //printf("changed ip\n");
    return ip_addr;
}


uint32_t get_ip(char* ip)
{
    uint32_t ip_addr = 0;
    uint32_t mask = 0;
    string append;

    for(int i =0; i < strlen(ip); ++i)
    {
        if(ip[i] == '.' || ip[i] == '\\' || i == strlen(ip) - 1)
        {
            if(i == strlen(ip) - 1 || ip[i] == '\\')
            {
                append += ip[i];
            }
            mask = (unsigned char)atoi(append.c_str());
            ip_addr = ip_addr | mask; 
            if(i != strlen(ip) -1 && ip[i] != '\\')
                ip_addr = ip_addr << 8;
            // printf("%s\n", append.c_str());
            // printf("%X\n", ip_addr);
            append = "";
        }
        else
        {
            append += ip[i];
        }
    }
    //printf("changed ip\n");
    return ip_addr;
}

void printNode(Node DVnode)
{
    printf("Ip: %X, cost: %d, next_hop: %d\n", DVnode.ip, DVnode.cost, DVnode.next_hop);
}