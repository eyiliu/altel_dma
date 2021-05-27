
#include <unistd.h>
#include <vector>
#include <iostream>

#include "TcpServer.hh"
#include "TcpConnection.hh"

TcpServer::TcpServer(short int port, FunProcessMessage recvFun){
  m_isActive = true;
  m_fut = std::async(std::launch::async, &TcpServer::threadClientMananger, &m_isActive, port, recvFun);
}

TcpServer::~TcpServer(){
  printf("TcpServer deconstructing\n");
  if(m_fut.valid()){
    m_isActive = false;
    m_fut.get();
  }
  printf("TcpServer deconstruction done\n");
}

uint64_t TcpServer::threadClientMananger(bool* isTcpServ, short int port, FunProcessMessage recvFun){
  *isTcpServ = true;
  int sockfd = TcpConnection::createServerSocket(port);
  if(sockfd<0){
    printf("unable to create server listenning socket\n");
    *isTcpServ = false;
  }

  printf("creating pool for incomming client connections\n");
  std::vector<std::unique_ptr<TcpConnection>> tcpConns;

  while(*isTcpServ){
    auto newConn = TcpConnection::waitForNewClient(sockfd, std::chrono::seconds(1), recvFun);
    if(newConn){
      printf("add new client connection\n");
      tcpConns.push_back(std::move(newConn));
    }

    for(auto itConnUP = tcpConns.begin(); itConnUP != tcpConns.end(); ++itConnUP){
      if(*itConnUP && !(**itConnUP)){
        itConnUP->reset();
      }
    }
    bool isNeedUpdate = true;
    while(isNeedUpdate){
      isNeedUpdate = false;
      for(auto itConnUP = tcpConns.begin(); itConnUP != tcpConns.end(); ++itConnUP){
        if(!(*itConnUP)){
          tcpConns.erase(itConnUP);
          isNeedUpdate = true;
          break;
        }
      }
    }
  }
  printf("removing all client connections...\n");
  tcpConns.clear();
  if(sockfd>=0){
    close(sockfd);
  }
  printf("server is closed\n");
  return 0;
}
