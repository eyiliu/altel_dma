#include <cstdio>
#include <cstring>
#include <cstdlib>

#include <future>
#include <chrono>
#include <memory>
#include <vector>

#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/socket.h>



struct TcpServerConn{
  sockaddr_in sockaddr_conn;
  std::future<uint64_t> fut;
  bool isRunning;

  TcpServerConn() = delete;
  TcpServerConn(const TcpServerConn&) =delete;
  TcpServerConn& operator=(const TcpServerConn&) =delete;
  TcpServerConn(altel::Layer* layer, int sockfd_conn, sockaddr_in sockaddr_conn_){
    sockaddr_conn = sockaddr_conn_;
    isRunning = true;
    fut = std::async(std::launch::async, &TcpServerConn::AsyncTcpConn, &isRunning, layer, sockfd_conn);
  }
  ~TcpServerConn(){
    printf("TcpServerConn deconstructing\n");
    if(fut.valid()){
      isRunning = false;
      fut.get();
    }
    printf("TcpServerConn deconstruction done\n");
  }

  static uint64_t AsyncTcpConn(bool* isTcpConn, altel::Layer* layer, int sockfd){
    printf("AsyncTcpServerConn is started\n");
    uint64_t n_ev = 0;
    *isTcpConn = true;
    while (*isTcpConn){
      auto &ev_front = layer->Front();
      if(ev_front){
        auto ev = ev_front;
        layer->PopFront();
        std::string ev_raw = ev->m_raw;
        char *writeptr = ev_raw.empty()?nullptr:&ev_raw[0];
        size_t bytes_read = ev_raw.size();
        while (bytes_read > 0 && *isTcpConn) {
          int written = write(sockfd, writeptr, bytes_read);
          if (written < 0) {
            fprintf(stderr, "ERROR writing to the TCP socket. errno: %d\n", errno);
            if (errno == EPIPE) {
              *isTcpConn = false;
              break;
            }
          } else {
            bytes_read -= written;
            writeptr += written;
          }
        }
        n_ev++;
      }
      else{
        std::this_thread::sleep_for(std::chrono::microseconds(1));
        continue;
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

  TcpServer(altel::Layer* layer, short int port){
    isRunning = true;
    fut = std::async(std::launch::async, &TcpServer::AsyncTcpServer, &isRunning, layer, port);
  }
  ~TcpServer(){
    printf("TcpServer deconstructing\n");
    if(fut.valid()){
      isRunning = false;
      fut.get();
    }
    printf("TcpServer deconstruction done\n");
  }

  static uint64_t AsyncTcpServer(bool* isTcpServ, altel::Layer* layer, short int port){
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
          continue;
        }
        fprintf(stderr, "ERROR on accept \n");
        throw;
      }

      tcpConns.push_back(std::make_unique<TcpServerConn>(layer, sockfd_conn, cli_addr));
      printf("new connection from %03d.%03d.%03d.%03d\n",
             (cli_addr.sin_addr.s_addr & 0xFF), (cli_addr.sin_addr.s_addr & 0xFF00) >> 8,
             (cli_addr.sin_addr.s_addr & 0xFF0000) >> 16, (cli_addr.sin_addr.s_addr & 0xFF000000) >> 24);

      // for(auto & conn: tcpConns){
      //   if(conn && conn->isRunning){
      //     conn.reset();
      //   }
      // }// TODO: erase
    }

    printf("AsyncTcpServ is removing connections...\n");
    tcpConns.clear();
    printf("AsyncTcpServ is exited\n");

    return 0;
  }
};


struct TcpClientConn{
  std::future<uint64_t> fut;
  bool isRunning;

  TcpClientConn() = delete;
  TcpClientConn(const TcpClientConn&) =delete;
  TcpClientConn& operator=(const TcpClientConn&) =delete;
  TcpClientConn(const std::string& host,  short int port){
    isRunning = true;
    fut = std::async(std::launch::async, &TcpClientConn::AsyncTcpConn, &isRunning, host, port);
  }
  ~TcpClientConn(){
    printf("TcpClientConn deconstructing\n");
    if(fut.valid()){
      isRunning = false;
      fut.get();
    }
    printf("TcpClientConn deconstruction done\n");
  }

  static uint64_t AsyncTcpConn(bool* isTcpConn, const std::string& host, short int port){
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
      return -1;
    }

    uint64_t n_ev = 0;
    *isTcpConn = true;
    while (*isTcpConn){
      std::string raw_pack = readPack(sockfd, std::chrono::milliseconds(100));
      if(!raw_pack.empty()){
        printf("Client got raw pack: %s\n", StringToHexString(raw_pack).c_str());
        auto df =  std::make_shared<altel::DataFrame>(std::move(raw_pack));
      }
    }

    close(sockfd);
    *isTcpConn = false;
    printf("AsyncTcpClientConn exited\n");
    return n_ev;
  }

  static std::string CStringToHexString(const char *bin, const int len){
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

  static std::string StringToHexString(const std::string& bin){
    return CStringToHexString(bin.data(), bin.size());
  }

  static std::string readPack(int fd_rx, const std::chrono::milliseconds &timeout_idle){ //timeout_read_interval
    size_t size_buf_min = 16;
    size_t size_buf = size_buf_min;
    std::string buf(size_buf, 0);
    size_t size_filled = 0;
    std::chrono::system_clock::time_point tp_timeout_idle;
    bool can_time_out = false;
    int read_len_real = 0;
    while(size_filled < size_buf){
      fd_set fds;
      timeval tv_timeout;
      FD_ZERO(&fds);
      FD_SET(fd_rx, &fds);
      FD_SET(0, &fds);
      tv_timeout.tv_sec = 0;
      tv_timeout.tv_usec = 10;
      if( !select(fd_rx+1, &fds, NULL, NULL, &tv_timeout) || !FD_ISSET(fd_rx, &fds) ){
        if(!can_time_out){
          can_time_out = true;
          tp_timeout_idle = std::chrono::system_clock::now() + timeout_idle;
        }
        else if(std::chrono::system_clock::now() > tp_timeout_idle){
          if(size_filled == 0){
            // std::fprintf(stderr, "WARNING<%s>: timeout error of empty data reading \n", __func__ );
            return std::string();
          }
          else{
            std::fprintf(stderr, "ERROR<%s>: timeout error of incomplete data reading \n", __func__ );
            return std::string();
          }
        }
        continue; // loop untill timeout or data avalible
      }
      //we should have data by now.
      // read_len_real = recv(fd_rx, &buf[0], (unsigned int)(size_buf), MSG_WAITALL);
      // return std::string();

      read_len_real = recv(fd_rx, &buf[size_filled], (unsigned int)(size_buf-size_filled), MSG_WAITALL);
      if(read_len_real>0){
        size_filled += read_len_real;
        can_time_out = false;
        if(size_buf == size_buf_min  && size_filled >= size_buf_min){
          uint8_t header_byte =  buf.front();
          uint32_t w1 = *reinterpret_cast<const uint32_t*>(buf.data()+4);
          uint32_t size_payload = (w1 & 0xfffff);
          if(header_byte != 0x5a){
            std::fprintf(stderr, "ERROR<%s>: wrong header of data frame, skip\n", __func__);
            std::fprintf(stderr, "RawData_TCP_RX:\n%s\n", StringToHexString(buf).c_str());
            std::fprintf(stderr, "<");
            //TODO: skip broken data
            //dumpBrokenData(fd_rx);
            size_buf = size_buf_min;
            size_filled = 0;
            can_time_out = false;
            return std::string();
          }
          size_buf += size_payload;
          size_buf &= -4; // aligment 32bits, tail 32bits might be cutted.
          if(size_buf > 300){
            size_buf = 300;
          }
          buf.resize(size_buf);
        }
      }
      else if( read_len_real == 0){
        continue;
      }
      else{
        std::fprintf(stderr, "ERROR<%s>: read(...) returns error code %d\n", __func__,  read_len_real);
        throw;
      }
    }
    return buf;
  }
};
