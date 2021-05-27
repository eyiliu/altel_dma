
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

  std::future<int64_t> fut_send;
  
  while(m_isActive){
    if( (!m_conn) || (m_conn && !(*m_conn)) ){
      auto new_conn = TcpConnection::waitForNewClient(sockfd, std::chrono::seconds(1), &TcpServer::processMessage);
      if(new_conn){
	m_conn = std::move(new_conn);
	fut_send = std::async(std::launch::async, &TcpServer::threadConnectionSend, this);
      }
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(1000));
  }

  if(sockfd>=0){
    close(sockfd);
  }
  printf("server is closed\n");
  return 0;
}

int TcpServer::processMessage(msgpack::object_handle &oh){ // for command
  std::cout << "TcpServer processMessage: " << oh.get() << std::endl;
  return 0;
}

int64_t TcpServer::threadConnectionSend(){
  while(m_isActive){
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
    if(m_conn){
      m_conn->sendRaw(strbuf.data(), strbuf.size());
    }
  }
  return 0; 
}
