#pragma once

#include <cstdio>
#include <cstring>
#include <cstdlib>

#include <future>

#include <signal.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <chrono>
#include <filesystem>

#include "TcpConnection.hh"
#include "AltelReader.hh"
#include "FirmwarePortal.hh"

class TcpServer{
public:
  TcpServer() = delete;
  TcpServer(const TcpServer&) =delete;
  TcpServer& operator=(const TcpServer&) =delete;
  TcpServer(short int port);
  ~TcpServer();

  uint64_t threadClientMananger(short int port);
  int64_t threadConnectionSend();

  static int processMessage(msgpack::object_handle &oh);

private:
  std::future<uint64_t> m_fut;
  bool m_isActive{false};
  FirmwarePortal m_fw{"builtin"};
  AltelReader m_rd;
  std::unique_ptr<TcpConnection> m_conn;
};

