
#include "TcpServer.hh"
#include <iostream>

int main(){
  std::unique_ptr<TcpServer> tcpServer(new TcpServer(9000));
  std::cout<<"sleep 1000s"<<std::endl;
  std::this_thread::sleep_for (std::chrono::seconds(1000));
  return 0;
}
