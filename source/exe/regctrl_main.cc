#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <iostream>

#include <signal.h>


#include "Layer.hh"

#include "getopt.h"
#include "linenoise.h"

#include "TcpServ.hh"

template<typename ... Args>
static std::string FormatString( const std::string& format, Args ... args ){
  std::size_t size = std::snprintf( nullptr, 0, format.c_str(), args ... ) + 1;
  std::unique_ptr<char[]> buf( new char[ size ] );
  std::snprintf( buf.get(), size, format.c_str(), args ... );
  return std::string( buf.get(), buf.get() + size - 1 );
}

namespace{
  std::string LoadFileToString(const std::string& path){
    std::ifstream ifs(path);
    if(!ifs.good()){
      std::cerr<<"LoadFileToString:: ERROR, unable to load file<"<<path<<">\n";
      throw;
    }

    std::string str;
    str.assign((std::istreambuf_iterator<char>(ifs) ),
               (std::istreambuf_iterator<char>()));
    return str;
  }
}


static  const std::string help_usage
(R"(
Usage:
-h : print usage information, and then quit

'help' command in interactive mode provides detail usage information
)"
  );


static  const std::string help_usage_linenoise
(R"(

keyword: help, info, quit, sensor, firmware, set, get, init, start, stop, reset, regcmd
example:
  A) init  (set firmware and sensosr to ready-run state after power-cirlce)
   > init

  B) start (set firmware and sensosr to running state from ready-run state)
   > start

  C) stop  (set firmware and sensosr to stop-run state)
   > stop


  1) get firmware regiester
   > firmware get FW_REG_NAME

  2) set firmware regiester
   > firmware set FW_REG_NAME 10

  3) get sensor regiester
   > sensor get SN_REG_NAME

  4) set sensor regiester
   > sensor set SN_REG_NAME 10

  5) exit/quit command line
   > quit

)"
  );

struct DummyDump;

int main(int argc, char **argv){
  std::string c_opt;
  int c;
  while ( (c = getopt(argc, argv, "h")) != -1) {
    switch (c) {
    case 'h':
      fprintf(stdout, "%s", help_usage.c_str());
      return 0;
      break;
    default:
      fprintf(stderr, "%s", help_usage.c_str());
      return 1;
    }
  }

  ///////////////////////
  std::unique_ptr<altel::Layer> layer;
  std::unique_ptr<TcpServer> tcpServer;
  std::unique_ptr<DummyDump> dummyDump;
  std::unique_ptr<TcpClientConn> tcpClient;

  const char* linenoise_history_path = "/tmp/.alpide_cmd_history";
  linenoiseHistoryLoad(linenoise_history_path);
  linenoiseSetCompletionCallback([](const char* prefix, linenoiseCompletions* lc)
                                   {
                                     static const char* examples[] =
                                       {"help", "quit", "exit", "info",
                                        "init", "start", "stop",
                                        "sensor", "firmware", "set", "get",
                                        "tcpserver", "tcpclient", "dump",
                                        NULL};
                                     size_t i;
                                     for (i = 0;  examples[i] != NULL; ++i) {
                                       if (strncmp(prefix, examples[i], strlen(prefix)) == 0) {
                                         linenoiseAddCompletion(lc, examples[i]);
                                       }
                                     }
                                   } );


  const char* prompt = "\x1b[1;32malpide\x1b[0m> ";
  while (1) {
    char* result = linenoise(prompt);
    if (result == NULL) {
      if(linenoiseKeyType()==1){
        if(layer){
          printf("stopping\n");
          layer->fw_stop();
          layer->rd_stop();
          printf("done\n");
        }
        continue;
      }
      break;
    }
    if ( std::regex_match(result, std::regex("\\s*((?:quit)|(?:exit))\\s*")) ){
      printf("quiting \n");
      linenoiseHistoryAdd(result);
      free(result);
      break;
    }

    if ( std::regex_match(result, std::regex("\\s*(help)\\s*")) ){
      fprintf(stdout, "%s", help_usage_linenoise.c_str());
    }
    else if ( std::regex_match(result, std::regex("\\s*(init)\\s*")) ){
      printf("initializing\n");
      dummyDump.reset();
      tcpClient.reset();
      tcpServer.reset();
      layer.reset(new altel::Layer());
      layer->fw_init();
      printf("done\n");
    }
    else if ( std::regex_match(result, std::regex("\\s*(start)\\s*")) ){
      if(layer){
        printf("starting\n");
        layer->rd_start();
        layer->fw_start();
        printf("done\n");
      }
    }
    else if ( std::regex_match(result, std::regex("\\s*(stop)\\s*")) ){
      if(layer){
        printf("stopping\n");
        layer->fw_stop();
        layer->rd_stop();
        printf("done\n");
      }
    }
    else if ( std::regex_match(result, std::regex("\\s*(info)\\s*"))){
      if(layer)
        std::cout<< layer->GetStatusString()<<std::endl;
    }
    else if ( std::regex_match(result, std::regex("\\s*(dump)\\s+(start)\\s*"))){
      if(layer)
        dummyDump = std::make_unique<DummyDump>(layer.get());

    }
    else if ( std::regex_match(result, std::regex("\\s*(dump)\\s+(stop)\\s*"))){
      dummyDump.reset();
    }
    else if ( std::regex_match(result, std::regex("\\s*(tcpserver)\\s+(start)\\s*"))){
      if(layer)
        tcpServer = std::make_unique<TcpServer>(layer.get(), 9000);
    }
    else if ( std::regex_match(result, std::regex("\\s*(tcpserver)\\s+(stop)\\s*"))){
      tcpServer.reset();
    }
    else if ( std::regex_match(result, std::regex("\\s*(tcpclient)\\s+(start)\\s*"))){
        tcpClient = std::make_unique<TcpClientConn>("127.0.0.1", 9000);
    }
    else if ( std::regex_match(result, std::regex("\\s*(tcpclient)\\s+(stop)\\s*"))){
      tcpClient.reset();
    }

    else if ( std::regex_match(result, std::regex("\\s*(sensor)\\s+(set)\\s+(\\w+)\\s+(?:(0[Xx])?([0-9]+))\\s*")) ){
      std::cmatch mt;
      std::regex_match(result, mt, std::regex("\\s*(sensor)\\s+(set)\\s+(\\w+)\\s+(?:(0[Xx])?([0-9]+))\\s*"));
      std::string name = mt[3].str();
      if(layer && layer->m_fw){
        uint64_t value = std::stoull(mt[5].str(), 0, mt[4].str().empty()?10:16);
        layer->m_fw->SetAlpideRegister(name, value);
      }
    }
    else if ( std::regex_match(result, std::regex("\\s*(sensor)\\s+(get)\\s+(\\w+)\\s*")) ){
      std::cmatch mt;
      std::regex_match(result, mt, std::regex("\\s*(sensor)\\s+(get)\\s+(\\w+)\\s*"));
      std::string name = mt[3].str();
      if(layer && layer->m_fw){
        uint64_t value = layer->m_fw->GetAlpideRegister(name);
        fprintf(stderr, "%s = %llu, %#llx\n", name.c_str(), value, value);
      }
    }
    else if ( std::regex_match(result, std::regex("\\s*(firmware)\\s+(set)\\s+(\\w+)\\s+(?:(0[Xx])?([0-9]+))\\s*")) ){
      std::cmatch mt;
      std::regex_match(result, mt, std::regex("\\s*(firmware)\\s+(set)\\s+(\\w+)\\s+(?:(0[Xx])?([0-9]+))\\s*"));
      std::string name = mt[3].str();
      if(layer && layer->m_fw){
        uint64_t value = std::stoull(mt[5].str(), 0, mt[4].str().empty()?10:16);
        layer->m_fw->SetFirmwareRegister(name, value);
      }
    }
    else if ( std::regex_match(result, std::regex("\\s*(firmware)\\s+(get)\\s+(\\w+)\\s*")) ){
      std::cmatch mt;
      std::regex_match(result, mt, std::regex("\\s*(firmware)\\s+(get)\\s+(\\w+)\\s*"));
      std::string name = mt[3].str();
      if(layer && layer->m_fw){
        uint64_t value = layer->m_fw->GetFirmwareRegister(name);
        fprintf(stderr, "%s = %llu, %#llx\n", name.c_str(), value, value);
      }
    }
    else{
      std::fprintf(stderr, "unknown command<%s>! consult possible commands by help....\n", result);
      linenoisePreloadBuffer("help");
    }

    linenoiseHistoryAdd(result);
    free(result);
  }

  linenoiseHistorySave(linenoise_history_path);
  linenoiseHistoryFree();

  printf("resetting from main thread.");
  dummyDump.reset();
  tcpClient.reset();
  tcpServer.reset();
  layer.reset();

  return 0;
}



struct DummyDump{
  std::future<uint64_t> fut;
  bool isRunning;

  DummyDump() = delete;
  DummyDump(const DummyDump&) =delete;
  DummyDump& operator=(const DummyDump&) =delete;
  DummyDump(altel::Layer *layer){
    isRunning = true;
    fut = std::async(std::launch::async, &DummyDump::AsyncDump, &isRunning, layer);
  }
  ~DummyDump(){
    if(fut.valid()){
      isRunning = false;
      fut.get();
    }
  }

  static uint64_t AsyncDump(bool* isDumping, altel::Layer* layer){
    auto now = std::chrono::system_clock::now();
    auto now_c = std::chrono::system_clock::to_time_t(now);
    // std::string now_str = TimeNowString("%y%m%d%H%M%S");

    uint64_t n_ev = 0;
    *isDumping = true;
    while (*isDumping){
      auto &ev_front = layer->Front();
      if(ev_front){
        // ev_sync.push_back(ev_front);
        layer->PopFront();
        n_ev++;
      }
      else{
        std::this_thread::sleep_for(std::chrono::microseconds(1));
        continue;
      }
    }
    printf("AsyncDump exited\n");
    return n_ev;
  }

};
