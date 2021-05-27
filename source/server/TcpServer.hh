#pragma once

#include <cstdio>
#include <cstring>
#include <cstdlib>

#include <future>

#include "TcpConnection.hh"

class TcpServer{
public:
  TcpServer() = delete;
  TcpServer(const TcpServer&) =delete;
  TcpServer& operator=(const TcpServer&) =delete;
  TcpServer(short int port, FunProcessMessage recvFun);
  ~TcpServer();

  static uint64_t threadClientMananger(bool* isTcpServ, short int port, FunProcessMessage recvFun);

private:
  std::future<uint64_t> m_fut;
  bool m_isActive;
};

