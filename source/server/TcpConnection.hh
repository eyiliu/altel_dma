#pragma once

#include <cstdio>
#include <cstring>
#include <cstdlib>

#include <string>
#include <future>
#include <memory>
#include <chrono>

#include <msgpack.hpp>
typedef msgpack::unique_ptr<msgpack::zone> unique_zone;

class TcpConnection;

//callback
typedef int (*FunProcessMessage)(msgpack::object_handle& );

class TcpConnection{
public:
  TcpConnection() = delete;
  TcpConnection(const TcpConnection&) =delete;
  TcpConnection& operator=(const TcpConnection&) =delete;
  TcpConnection(int sockfd, FunProcessMessage recvFun);

  ~TcpConnection();

  operator bool() const;

  //forked thread
  uint64_t threadConnRecv(FunProcessMessage recvFun);

  void sendRaw(const char* buf, size_t len);

  static int createSocket();
  static void setupSocket(int& sockfd);
  static void closeSocket(int& sockfd);

  //client side
  static std::unique_ptr<TcpConnection> connectToServer(const std::string& host,  short int port, FunProcessMessage recvFun);
  //server side
  static int createServerSocket(short int port);
  static std::unique_ptr<TcpConnection> waitForNewClient(int sockfd, const std::chrono::milliseconds &timeout, FunProcessMessage recvFun);

  //test
  static int processMessageServerTest(msgpack::object_handle& oh);
  static int processMessageClientTest(msgpack::object_handle& oh);

private:
  std::future<uint64_t> m_fut;
  bool m_isAlive{false};
  int m_sockfd{-1};
};
