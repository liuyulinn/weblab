#include "switch.h"

SwitchBase* CreateSwitchObject() {
  // TODO : Your code.
  return new Switch;
}

void Switch::InitSwitch(int numPorts)
{
  num_ports = numPorts;
  return;
}

int Switch::ProcessFrame(int inPort, char* framePtr)
{
  // printf("************\n");
  // printf("recvmessage from inPort %d, print present table: \n", inPort);
  // for(int i = 0; i < Table.size(); i ++)
  // {
  //   printf("%X:%X:%X:%X:%X:%X port: %d time: %u\n", Table[i].mac_addr[0],Table[i].mac_addr[1],Table[i].mac_addr[2],Table[i].mac_addr[3],
  //   Table[i].mac_addr[4],Table[i].mac_addr[5], Table[i].port, Table[i].timestep);
  // }
  // printf("end of table!\n");
  ether_header_t* header = (ether_header_t*) framePtr;
  uint16_t type = header->ether_type;
  if(type == ETHER_DATA_TYPE)
  {
    Update(inPort, header->ether_src);
    int tmp = Find(header->ether_dest);
    if(tmp == -1) return 0;
    else if(inPort == Table[tmp].port) return -1;
    else return Table[tmp].port;
  }
  else
  {
    Aging();
    return -1;
  }
}

int Switch::Find(mac_addr_t dst)
{
  for(int i = 0; i < Table.size(); ++i)
  {
    int flag = 1;
    for(int j = 0; j < 6; ++j)
    {
      if(Table[i].mac_addr[j] != dst[j])
      {
        flag = 0;
        break;
      }
    }
    if(flag) return i;
  }
  return -1; // not found
}

void Switch::Update(int inPort, mac_addr_t src)
{
  int tmp = Find(src);
  if(tmp == -1)
  {
    Node new_node;
    memcpy(&new_node.mac_addr, src, sizeof(mac_addr_t));
    new_node.port = inPort;
    new_node.timestep = ETHER_MAC_AGING_THRESHOLD;
    Table.push_back(new_node);
    //printf("update!\n");
    //printf("%X:%X:%X:%X:%X:%X port: %d time: %u\n", new_node.mac_addr[0], new_node.mac_addr[1], new_node.mac_addr[2], new_node.mac_addr[3],
    //new_node.mac_addr[4],new_node.mac_addr[5], new_node.port, new_node.timestep);
    //printf("successful update!\n");
    return;
  }
  else
  {
    //printf("refresh!\n");
    Table[tmp].timestep = ETHER_MAC_AGING_THRESHOLD;
    return;
  }
}

void Switch::Aging()
{
  for(int i = Table.size() - 1; i >= 0; --i)
  {
    Table[i].timestep --;
    if(Table[i].timestep == ETHER_COMMAND_TYPE_AGING)
    {
      // 丢掉这一帧
      swap(Table[i], Table.back());
      Table.pop_back();
      //printf("drop out this port!\n");
    }
  }
  return;
}
