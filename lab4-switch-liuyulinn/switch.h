#ifndef COMPNET_LAB4_SRC_SWITCH_H
#define COMPNET_LAB4_SRC_SWITCH_H

#include "types.h"
#include<vector>
#include<string>

using namespace std;

class SwitchBase {
 public:
  SwitchBase() = default;
  ~SwitchBase() = default;

  virtual void InitSwitch(int numPorts) = 0;
  virtual int ProcessFrame(int inPort, char* framePtr) = 0;
};

extern SwitchBase* CreateSwitchObject();

// TODO : Implement your switch class.
struct Node{
  mac_addr_t mac_addr;
  int port;
  uint16_t timestep;
};
typedef struct Node* pnode;

class Switch: public SwitchBase {
public:
  void InitSwitch(int numPorts);
  int ProcessFrame(int inPort, char* framePtr);
  int Find(mac_addr_t dst);
  void Update(int inPort, mac_addr_t src);
  void Aging();
private:
  vector<Node> Table;
  int num_ports;
};

#endif  // ! COMPNET_LAB4_SRC_SWITCH_H
