
#include <unistd.h>
#include <vector>
#include <iostream>

#include "TcpServer.hh"
#include "TcpConnection.hh"

TcpServer::TcpServer(short int port){
  m_fw.fw_init();
  m_fw.fw_start();
  m_rd.Open();

  m_isActive = true;
  m_fut = std::async(std::launch::async, &TcpServer::threadClientMananger, this, port);
}

TcpServer::~TcpServer(){
  printf("TcpServer deconstructing\n");
  if(m_fut.valid()){
    m_isActive = false;
    m_fut.get();
  }
  printf("TcpServer deconstruction done\n");
}

uint64_t TcpServer::threadClientMananger(short int port){
  m_isActive = true;
  int sockfd = TcpConnection::createServerSocket(port);
  if(sockfd<0){
    printf("unable to create server listenning socket\n");
    m_isActive = false;
  }

  while(m_isActive){
    if( (!m_conn) || (m_conn && !(*m_conn)) ){
      auto new_conn = TcpConnection::waitForNewClient(sockfd, std::chrono::seconds(1),
                                                      reinterpret_cast<FunProcessMessage>(&TcpServer::processMessage),
                                                      reinterpret_cast<FunSendDeamon>(&TcpServer::perConnSendDeamon), this);
      if(new_conn){
        m_conn = std::move(new_conn);
      }
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(1000));
  }

  if(sockfd>=0){
    TcpConnection::closeSocket(sockfd);
  }
  printf("server is closed\n");
  return 0;
}

struct NetMsg{
  uint16_t msgtype;
  uint16_t device;
  uint32_t address;
  uint32_t value;
  std::vector<char> bin;
  MSGPACK_DEFINE(msgtype, device, address, value, bin);
};

std::string binToHexString(const char *bin, int len){
  constexpr char hexmap[] = {'0', '1', '2', '3', '4', '5', '6', '7',
                             '8', '9', 'a', 'b', 'c', 'd', 'e', 'f'};
  const unsigned char* data = (const unsigned char*)(bin);
  std::string s(len * 2, ' ');
  for (int i = 0; i < len; ++i) {
    s[2 * i]     = hexmap[(data[i] & 0xF0) >> 4];
    s[2 * i + 1] = hexmap[data[i] & 0x0F];
  }
  return s;
}

int TcpServer::processMessage(void* pobj, void* pconn, msgpack::object_handle &oh){
  std::cout << "TcpServer processMessage: " << oh.get() << std::endl;
  auto& msg = oh.get();
  NetMsg netmsg = msg.as<NetMsg>();
  std::cout<< netmsg.msgtype<<" " << netmsg.device<<" " <<netmsg.address <<" "<< netmsg.value<<" "<<std::endl;
  std::cout<< "bin "<<binToHexString(netmsg.bin.data(),netmsg.bin.size())<<std::endl;
  return 0;
}


int TcpServer::perConnSendDeamon(void  *pconn){
  // TcpServer* serv = reinterpret_cast<TcpServer*>(pobj);
  TcpConnection* conn = reinterpret_cast<TcpConnection*>(pconn);
  while(conn &&  (*conn)){
    std::string dataraw = m_rd.readRawPack(std::chrono::milliseconds(1000));
    if(dataraw.empty())
      continue;

    std::stringstream ssbuf;
    std::string strbuf;
    msgpack::packer<std::stringstream> pk(ssbuf);
    pk.pack_map(1);
    const std::string msgkey_data("data");
    pk.pack_str(msgkey_data.size());
    pk.pack_str_body(msgkey_data.data(), msgkey_data.size());
    pk.pack_bin(dataraw.size());
    pk.pack_bin_body(dataraw.data(), dataraw.size());
    strbuf = ssbuf.str();
    conn->sendRaw(strbuf.data(), strbuf.size());
  }
  return 0;
}
