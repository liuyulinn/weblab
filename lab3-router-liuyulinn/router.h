#include "router_prototype.h"
#include <vector>
#include "stdio.h"
#include "stdlib.h"
#include "arpa/inet.h"
#include <cstdint>
#include <cstring>
#include <string>

#define MAXPORT 512
#define MAXIP 1500
#define MAXALLOC 512

using namespace std;

struct Node{
    uint32_t ip;
    int cost;
    int next_hop;
    int range;
};
typedef struct Node* pnode;

struct head{
    uint32_t src;
    uint32_t dst;
    uint8_t type;
    uint16_t length;
};
typedef struct head* router_head;

uint8_t TYPE_DV = 0x00;
uint8_t TYPE_DATA = 0x01;
uint8_t TYPE_CONTROL = 0x02;

class NAT {
public:
    int inport(router_head rhead);
    int outport(router_head rhead);
    void release(uint32_t ip);
    uint32_t availableIp;
    int addrNum;
    uint32_t allocatedIp[MAXALLOC];
    int allocatedIpNum;
};


class Router : public RouterBase {
public:
    void router_init(int port_num, int external_port, char* external_addr, char* available_addr);
    int router(int in_port, char* packet);
    int isContain(uint32_t ip);
private:
    int portNum;
    uint32_t externalPort;
    int cost[MAXPORT];
    vector<Node> DV;
    NAT nat;
};

uint32_t get_ip(char* ip);
uint32_t get_ip_net(char* ip, int* num);
void printNode(Node DVnode);