#include <cstdio>
#include <cstring>
#include <cstdlib>

#include <iostream>
#include <future>
#include <chrono>
#include <memory>
#include <vector>

#include <unistd.h>
#include <signal.h>

#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <sys/socket.h>

#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>

#include <msgpack.hpp>
typedef msgpack::unique_ptr<msgpack::zone> unique_zone;

void  WriteWord(uint64_t address, uint64_t value){

  int fd;
  if ((fd = open("/dev/mem", O_RDWR|O_SYNC)) < 0 ) {
    printf("Error opening file. \n");
    exit(-1);
  }

  off_t phy_addr = address;
  size_t len = 1;
  size_t page_size = getpagesize();
  off_t offset_in_page = phy_addr & (page_size - 1);

  size_t mapped_size = page_size * ( (offset_in_page + len)/page_size + 1);
  void *map_base = mmap(NULL,
                        mapped_size,
                        PROT_READ | PROT_WRITE,
                        MAP_SHARED,
                        fd,
                        phy_addr & ~(off_t)(page_size - 1));

  if (map_base == MAP_FAILED){
    perror("Memory mapped failed\n");
    exit(-1);
  }

  char* virt_addr = (char*)map_base + offset_in_page;
  uint32_t* virt_addr_u32 = reinterpret_cast<uint32_t*>(virt_addr);
  *virt_addr_u32 = (uint32_t)value;

  printf("value %#lX", (uint32_t)value);

  close(fd);

};

uint64_t ReadWord(uint64_t address){
  int fd;
  if ((fd = open("/dev/mem", O_RDWR|O_SYNC)) < 0 ) {
    printf("Error opening file. \n");
    exit(-1);
  }
  off_t phy_addr = address;
  size_t len = 1;
  size_t page_size = getpagesize();
  off_t offset_in_page = phy_addr & (page_size - 1);

  size_t mapped_size = page_size * ( (offset_in_page + len)/page_size + 1);
  void *map_base = mmap(NULL,
                        mapped_size,
                        PROT_READ | PROT_WRITE,
                        MAP_SHARED,
                        fd,
                        phy_addr & ~(off_t)(page_size - 1));

  if (map_base == MAP_FAILED){
    perror("Memory mapped failed\n");
    exit(-1);
  }

  char* virt_addr = (char*)map_base + offset_in_page;
  uint32_t* virt_addr_u32 = reinterpret_cast<uint32_t*>(virt_addr);
  uint32_t reg_value = *virt_addr_u32;

  close(fd);
  return reg_value;
};


struct mesgCmd{
  std::string device;
  std::string method;
  uint32_t offset;
  uint32_t value;
  MSGPACK_DEFINE(device, method, offset, value);
};

struct TcpServerConn{
  sockaddr_in sockaddr_conn;
  std::future<uint64_t> fut;
  bool isRunning;

  TcpServerConn() = delete;
  TcpServerConn(const TcpServerConn&) =delete;
  TcpServerConn& operator=(const TcpServerConn&) =delete;
  TcpServerConn(int sockfd_conn, sockaddr_in sockaddr_conn_){
    sockaddr_conn = sockaddr_conn_;
    isRunning = true;
    fut = std::async(std::launch::async, &TcpServerConn::threadConn, &isRunning, sockfd_conn);
  }
  ~TcpServerConn(){
    printf("TcpServerConn deconstructing\n");
    if(fut.valid()){
      isRunning = false;
      fut.get();
    }
    printf("TcpServerConn deconstruction done\n");
  }

  static uint64_t threadConn(bool* isTcpConn, int sockfd){
    printf("AsyncTcpServerConn is started\n");
    uint64_t n_ev = 0;
    *isTcpConn = true;

    msgpack::unpacker unp;
    timeval tv_timeout;
    tv_timeout.tv_sec = 0;
    tv_timeout.tv_usec = 10;
    fd_set fds;
    while (*isTcpConn){
      FD_ZERO(&fds);
      FD_SET(sockfd, &fds);
      FD_SET(0, &fds);
      if(!select(sockfd+1, &fds, NULL, NULL, &tv_timeout) || !FD_ISSET(sockfd, &fds) ){
        continue;
      }
      unp.reserve_buffer(4096);
      int count = recv(sockfd, unp.buffer(), (unsigned int)(unp.buffer_capacity()), MSG_WAITALL);
      if(count== 0 && errno != EWOULDBLOCK && errno != EAGAIN){
        *isTcpConn = false; // closed connection
        std::printf("connection is closed by remote peer\n");
        break;
      }
      unp.buffer_consumed(count);
      msgpack::object_handle oh; // keep zone for reference.
      while (unp.next(oh)){
        msgpack::object msg = oh.get();
        std::cout << "message received: " << msg << std::endl;
        mesgCmd cmd = msg.as<mesgCmd>();
        std::cout<< cmd.device<<" " << cmd.method<<" " <<cmd.offset <<" "<< cmd.value<<std::endl;

        if(cmd.device == "memory" && cmd.method == "read"){
          uint64_t value = ReadWord(cmd.offset);
          

          
        }
        if(cmd.device == "memory" && cmd.method == "write"){
          std::cout<< "mem write,>>>>"<<std::endl;
          printf("addr %#lX,  value %#lX", cmd.offset , cmd.value);

          WriteWord(cmd.offset, cmd.value);
        }
// process_message(msg, life);
      }
      if(unp.nonparsed_size() > 4096) {
        throw std::runtime_error("nonparsed buffer of message is too large");
      }
    }

    close(sockfd);
    *isTcpConn = false;
    printf("AsyncTcpServerConn is exited\n");
    return n_ev;
  }
};


struct TcpServer{
  std::future<uint64_t> fut;
  bool isRunning;

  TcpServer() = delete;
  TcpServer(const TcpServer&) =delete;
  TcpServer& operator=(const TcpServer&) =delete;

  TcpServer(short int port){
    isRunning = true;
    fut = std::async(std::launch::async, &TcpServer::AsyncTcpServer, &isRunning, port);
  }
  ~TcpServer(){
    printf("TcpServer deconstructing\n");
    if(fut.valid()){
      isRunning = false;
      fut.get();
    }
    printf("TcpServer deconstruction done\n");
  }

  static uint64_t AsyncTcpServer(bool* isTcpServ, short int port){
    auto now = std::chrono::system_clock::now();
    auto now_c = std::chrono::system_clock::to_time_t(now);
    // std::string now_str = TimeNowString("%y%m%d%H%M%S");
    printf("AsyncTcpServ is starting...\n");

    int sockfd = socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK | SOCK_CLOEXEC, 0);
    if (sockfd < 0)
      fprintf(stderr, "ERROR opening socket");

    /*allow reuse the socket binding in case of restart after fail*/
    int itrue = 1;
    setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &itrue, sizeof(itrue));

    sockaddr_in serv_addr;
    bzero((char *) &serv_addr, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = INADDR_ANY;
    serv_addr.sin_port = htons(port);
    if (bind(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0)
      fprintf(stderr, "ERROR on binding. errno=%d\n", errno);
    listen(sockfd, 1);
    printf("AsyncTcpServ is listenning...\n");

    std::vector<std::unique_ptr<TcpServerConn>> tcpConns;

    *isTcpServ = true;
    while(*isTcpServ){
      sockaddr_in cli_addr;
      socklen_t clilen = sizeof(cli_addr);

      int sockfd_conn = accept(sockfd, (struct sockaddr *) &cli_addr, &clilen); //wait for the connection
      if (sockfd_conn < 0){
        if( errno == EAGAIN  || errno == EWOULDBLOCK){
          std::this_thread::sleep_for(std::chrono::milliseconds(100));

          for (auto itConnUP = tcpConns.begin();
               itConnUP != tcpConns.end(); ++itConnUP){
            if(*itConnUP && !(*itConnUP)->isRunning ){
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
          continue;
        }
        fprintf(stderr, "ERROR on accept \n");
        throw;
      }

      tcpConns.push_back(std::make_unique<TcpServerConn>(sockfd_conn, cli_addr));
      printf("new connection from %03d.%03d.%03d.%03d\n",
             (cli_addr.sin_addr.s_addr & 0xFF), (cli_addr.sin_addr.s_addr & 0xFF00) >> 8,
             (cli_addr.sin_addr.s_addr & 0xFF0000) >> 16, (cli_addr.sin_addr.s_addr & 0xFF000000) >> 24);
    }

    printf("AsyncTcpServ is removing connections...\n");
    tcpConns.clear();
    printf("AsyncTcpServ is exited\n");
    return 0;
  }
};


struct TcpConnnection{
  std::future<uint64_t> m_fut;
  bool m_isAlive{false};
  int m_sockfd{-1};

  TcpConnnection() = delete;
  TcpConnnection(const TcpConnnection&) =delete;
  TcpConnnection& operator=(const TcpConnnection&) =delete;

  TcpConnnection(const std::string& host,  short int port){
    m_sockfd = connectToServer(host, port);
    if(m_sockfd>0)
      m_fut = std::async(std::launch::async, &TcpConnnection::threadConn, this);
  }

  ~TcpConnnection(){
    printf("TcpClientConn deconstructing\n");
    if(m_fut.valid()){
      m_isAlive = false;
      m_fut.get();
    }
    printf("TcpClientConn deconstruction done\n");
  }

  operator bool() const {
    return m_isAlive;
  }

  static int connectToServer(const std::string& host,  short int port){
    auto now = std::chrono::system_clock::now();
    auto now_c = std::chrono::system_clock::to_time_t(now);
    printf("AsyncTcpClientConn is running...\n");

    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0)
      fprintf(stderr, "ERROR opening socket");

    sockaddr_in serv_addr;
    bzero((char *) &serv_addr, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(port);
    serv_addr.sin_addr.s_addr = inet_addr(host.c_str());
    if (connect(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0){
      if(errno != EINPROGRESS){
        std::fprintf(stderr, "ERROR<%s>: unable to start TCP connection, error code %i \n", __func__, errno);
      }
      if(errno == 29){
        std::fprintf(stderr, "ERROR<%s>: TCP open timeout \n", __func__);
      }
      printf("\nConnection Failed \n");
      close(sockfd);
      sockfd = -1;
    }
    return sockfd;
  }

  uint64_t threadConn(){
    msgpack::unpacker unp;
    timeval tv_timeout;
    tv_timeout.tv_sec = 0;
    tv_timeout.tv_usec = 10;
    fd_set fds;
    m_isAlive = true;
    while (m_isAlive){
      FD_ZERO(&fds);
      FD_SET(m_sockfd, &fds);
      FD_SET(0, &fds);
      if(!select(m_sockfd+1, &fds, NULL, NULL, &tv_timeout) || !FD_ISSET(m_sockfd, &fds) ){
        // std::fprintf(stderr, "WARNING<%s>: timeout error of empty data reading \n", __func__ );
        continue;
      }
      unp.reserve_buffer(4096);
      int count = recv(m_sockfd, unp.buffer(), (unsigned int)(unp.buffer_capacity()), MSG_WAITALL);
      if(count== 0 && errno != EWOULDBLOCK && errno != EAGAIN){
        m_isAlive = false; // closed connection
        std::printf("connection is closed by remote peer\n");
        break;
      }
      unp.buffer_consumed(count);
      msgpack::object_handle oh;
      while (unp.next(oh)) {
        msgpack::object msg = oh.get();
        // unique_zone& life = oh.zone();
        std::cout << "message received: " << msg << std::endl;
        //process_message(msg, life);
      }

      if(unp.nonparsed_size() > 4096) {
        m_isAlive = false;
        std::fprintf(stderr, "error: nonparsed buffer of message is too large\n");
        break;
      }
    }
    m_isAlive = false;
    close(m_sockfd);
    m_sockfd = -1;
    printf("AsyncTcpClientConn exited\n");
    return 0;
  }

  void sendRaw(const char* buf, size_t len){
    const char* remain_bufptr = buf;
    size_t remain_len = len;
    while (remain_len > 0 && m_isAlive){
      int written = write(m_sockfd, remain_bufptr, remain_len);
      if (written < 0) {
        fprintf(stderr, "ERROR writing to the TCP socket. errno: %d\n", errno);
        if (errno == EPIPE) {
          m_isAlive = false;
          break;
        }
      }
      else{
        remain_bufptr += written;
        remain_len -= written;
      }
    }
  }
};


int main(){
  std::unique_ptr<TcpServer> tcpServer(new TcpServer(9000));
  std::cout<<"sleep 1000s"<<std::endl;
  std::this_thread::sleep_for (std::chrono::seconds(1000));

  std::cout<<"sleep 1s"<<std::endl;
  std::this_thread::sleep_for (std::chrono::seconds(1));
  std::unique_ptr<TcpConnnection> tcpClient(new TcpConnnection("127.0.0.1", 9000));
  std::cout<<"sleep 1s"<<std::endl;
  std::this_thread::sleep_for (std::chrono::seconds(1));

  std::cout<<"pack test data"<<std::endl;

  mesgCmd testMesg{"memory", "write", 0x43c00000, 0xff000000};
  // msgpack::type::tuple<int, bool, std::string> src("mem", read, "example");

  std::stringstream buffer;
  msgpack::pack(buffer, testMesg);
  std::string str(buffer.str());
  std::cout<<"send 5 bytes"<<std::endl;
  tcpClient->sendRaw(str.data(), 5);
  std::this_thread::sleep_for (std::chrono::seconds(2));
  std::cout<<"send remainning bytes"<<std::endl;
  tcpClient->sendRaw(str.data()+5, str.size()-5);
  std::this_thread::sleep_for (std::chrono::seconds(2));
  return 0;
}
