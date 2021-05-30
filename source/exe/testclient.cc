#include "TcpConnection.hh"
#include <iostream>


struct ClientData{
  int data;
};

int perConnClientProcessMsg(void* ,void* pconn, msgpack::object_handle& oh){
  std::cout<<"perConnClientProcessMsg is called"<<std::endl;
  msgpack::object msg = oh.get();
  unique_zone& life = oh.zone();

  NetMsg netmsg = msg.as<NetMsg>();
  std::cout<< netmsg.type<<" " << netmsg.device<<" " <<netmsg.address <<" "<< netmsg.value<<" "<<std::endl;
  std::cout<< "bin "<<TcpConnection::binToHexString(netmsg.bin.data(),netmsg.bin.size())<<std::endl;

  switch(netmsg.type){
  case NetMsg::Type::data :{
    std::cout<< "yes, data"<<std::endl;
    break;
  }
  default:
    std::cout<< "unknown msg type"<<std::endl;
  }
  return 0;
}

int perConnClientSendDeamon(void* ,void* pconn){
  std::this_thread::sleep_for (std::chrono::seconds(1000));
  std::cout<< "perConnClientSendDeamon do nothing, and return"<<std::endl;
  return 0;
}


int main(){
  std::cout<<"sleep 1s"<<std::endl;
  std::this_thread::sleep_for (std::chrono::seconds(1));

  ClientData cdata;
  std::unique_ptr<TcpConnection> tcpClient = TcpConnection::connectToServer("131.169.133.170", 9000, &perConnClientProcessMsg, nullptr, &cdata);

  std::cout<<"sleep 1s"<<std::endl;
  std::this_thread::sleep_for (std::chrono::seconds(1));

  NetMsg testMesg{NetMsg::data, 255, 0x43c00000, 0xff000000, {char(1), char(2), char(3), char(4)} };

  NetMsg daqinitMsg{NetMsg::daqinit, 0, 0, 0, {}};
  NetMsg daqstartMsg{NetMsg::daqstart, 0, 0, 0, {}};

  std::stringstream ssbuf;
  std::string strbuf;

  ssbuf.str(std::string());
  msgpack::pack(ssbuf, daqinitMsg);
  strbuf = ssbuf.str();
  tcpClient->sendRaw(strbuf.data(), strbuf.size());

  std::cout<<"init cmd is send"<<std::endl;
  std::cout<<"sleep 2s"<<std::endl;
  std::this_thread::sleep_for (std::chrono::seconds(2));

  ssbuf.str(std::string());
  msgpack::pack(ssbuf, daqstartMsg);
  strbuf = ssbuf.str();
  tcpClient->sendRaw(strbuf.data(), strbuf.size());

  std::cout<<"start cmd is send"<<std::endl;

  std::cout<<"sleep 1000s"<<std::endl;
  std::this_thread::sleep_for (std::chrono::seconds(1000));
  return 0;
}
