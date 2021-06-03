#include <cstdio>
#include <cctype>
#include <cstdlib>
#include <cstring>

#include <iostream>
#include <fstream>
#include <regex>
#include <chrono>
#include <thread>

#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>


static const std::string help_usage = R"(
Usage:
triggerGen_test <addresss_base>

examples:
./triggerGen_test 0x43c00000
)";


#define ADDROFF_REQ_WAIT 0x00
#define ADDROFF_REQ      0x04
#define ADDROFF_TRIG_ID  0x08
#define ADDROFF_CNT_REQ  0x0c
#define ADDROFF_CNT_TRIG 0x10

#define SIZE_ADDR_MAP 0x18

using std::uint32_t;
using std::size_t;
using namespace std::chrono_literals;

uint32_t *preg_req_wait;
uint32_t *preg_req;
uint32_t *preg_trig_id;
uint32_t *preg_cnt_req;
uint32_t *preg_cnt_trig;

void test_trig_id_read_write(){
  std::this_thread::sleep_for(20ms);
  std::printf("\n\nchecking TRIG_ID write/read\n");
  uint32_t val = 0x4321;
  *preg_trig_id = val;
  std::this_thread::sleep_for(20ms);
  uint32_t valread = *preg_trig_id; 
  if(valread != val){
    std::printf("fail: ummatched TRIG_ID write and read value. write: %#010x, read: %#010x \n", val, valread);
  }
}

void test_req_wait_when_idle(){
  std::this_thread::sleep_for(20ms);
  std::printf("\n\nchecking REQ_WAIT after idle for a while\n");
  if((*preg_req_wait & 1) != 1){
    std::printf("fail: REQ_WAIT is not 1 before send first request\n");
    return;
  }
  *preg_req = 1;
  std::this_thread::sleep_for(100ms);
  if((*preg_req_wait & 1) != 1){
    std::printf("fail: REQ_WAIT is not 1 after 100ms idle \n");
  }
}

// void test_req_wait_when_frequent_request(){
//   std::this_thread::sleep_for(20ms);
//   std::printf("\n\nchecking REQ_WAIT 1us  requests\n");
//   size_t numReq = 1000;
//   size_t numReqWait_set=0;
//   for(size_t n = 0; n < numReq; n++ ){
//     *preg_req = 1;
//     std::this_thread::sleep_for(1us);
//     if((*preg_req_wait & 1) == 1){
//       numReqWait_set++;
//     }
//   }
//   std::printf("status: got %zu REQ_WAIT==1 after %zu requests\n", numReqWait_set, numReq);
// }

void test_cnt_req(){
  std::this_thread::sleep_for(20ms);
  std::printf("\n\nchecking CNT_REQ counting up\n");
  size_t numMaxReq = 1000;
  size_t numReq = 1000;
  auto tp_start = std::chrono::steady_clock::now();
  auto tp_timeout = tp_start + 2s; 
  bool isTimeout = false;
  uint32_t old_cnt_req = *preg_cnt_req;
  for(size_t n = 0; n < numMaxReq; n++ ){
    while((*preg_req_wait & 1) != 1){
      std::chrono::steady_clock::now()>tp_timeout;
      isTimeout = true;
      numReq = n; 
      std::printf("fail: timeout during waiting of REQ_WAIT==1, before request #%zu\n", n);
      break;
    }
    if(isTimeout){
      break;
    }
    uint32_t old_cnt_req = *preg_cnt_req;
    *preg_req = 1;
    uint32_t expected_new_cnt_req = old_cnt_req+1;
    while( *preg_cnt_req != expected_new_cnt_req){
      std::chrono::steady_clock::now()>tp_timeout;
      isTimeout = true;
      numReq = n+1;
      std::printf("fail: timeout during waiting of CNT_REQ++, after request #%zu\n", n);
      break;
    }
    if(isTimeout){
      break;
    }    
  }
  uint32_t new_cnt_req = *preg_cnt_req ;
  
  if(isTimeout){
    std::printf("fail: unable to send all %zu requests. only %zu requests are sent.\n",
		numMaxReq, numReq);
  }
  if(new_cnt_req-old_cnt_req != numReq){
    std::printf("fail: %zu requests are sent, but CNT_REQ+=%u\n",
		numReq, new_cnt_req-old_cnt_req);
  }
}

void test_cnt_trig(){
  std::this_thread::sleep_for(20ms);
  std::printf("\n\nchecking CNT_TRIG counting up\n");
  size_t numMaxReq = 1000;
  size_t numReq = 1000;
  auto tp_start = std::chrono::steady_clock::now();
  auto tp_timeout = tp_start + 2s; 
  bool isTimeout = false;
  uint32_t old_cnt_req = *preg_cnt_req;
  uint32_t old_cnt_trig = *preg_cnt_trig;
  for(size_t n = 0; n < numMaxReq; n++ ){
    while((*preg_req_wait & 1) != 1){
      std::chrono::steady_clock::now()>tp_timeout;
      isTimeout = true;
      numReq = n; 
      std::printf("fail: timeout during waiting of REQ_WAIT==1, before request #%zu\n", n);
      break;
    }
    if(isTimeout){
      break;
    }
    uint32_t old_cnt_req = *preg_cnt_req;
    *preg_req = 1;
    uint32_t expected_new_cnt_req = old_cnt_req+1;
    while( *preg_cnt_req != expected_new_cnt_req){
      std::chrono::steady_clock::now()>tp_timeout;
      isTimeout = true;
      numReq = n+1;
      std::printf("fail: timeout during waiting of CNT_REQ++, after request #%zu\n", n);
      break;
    }
    if(isTimeout){
      break;
    }
  }
  uint32_t new_cnt_req = *preg_cnt_req ;
  if(isTimeout){
    std::printf("fail: unable to send all %zu requests. only %zu requests are sent.\n",
		numMaxReq, numReq);
  }
  if(new_cnt_req-old_cnt_req != numReq){
    std::printf("fail: %zu requests are sent, but CNT_REQ+=%u\n",
		numReq, new_cnt_req-old_cnt_req);
  }
  uint32_t new_cnt_trig = *preg_cnt_trig ;
  if(new_cnt_trig-old_cnt_trig != new_cnt_req-old_cnt_req){
    std::printf("warning: CNT_REQ+=%u, but CNT_TRIG+=%u\n",
		new_cnt_req-old_cnt_req, new_cnt_trig-old_cnt_trig);
  }  
}



void test_seq_id_trig(){
  std::this_thread::sleep_for(20ms);
  std::printf("\n\nchecking sequence REQ_ID and CNT_TRIG counting up\n");
  size_t numMaxReq = 1000;
  size_t numReq = 0;
  auto tp_start = std::chrono::steady_clock::now();
  auto tp_timeout = tp_start + 2s; 
  bool isTimeout = false;
  uint32_t old_cnt_req = *preg_cnt_req;
  uint32_t old_cnt_trig = *preg_cnt_trig;
  
  // uint32_t begin_trig_id = *preg_trig_id;
  uint32_t begin_trig_id = 0;
  for(size_t n = 0; n < numMaxReq; n++ ){

    // increase trigger id  +1
    *preg_trig_id = begin_trig_id + n;
    
    numReq = n;
    while((*preg_req_wait & 1) != 1){
      std::chrono::steady_clock::now()>tp_timeout;
      isTimeout = true;
      break;
    }
    if(isTimeout){
      std::printf("fail: timeout during waiting of REQ_WAIT==1, before request #%zu\n", n);
      std::printf("fail: unable to send all %zu requests. only %zu requests are sent.\n",
		  numMaxReq, numReq);
      break;
    }

    //new req
    uint32_t old_cnt_req = *preg_cnt_req;
    *preg_req = 1;
    numReq = n+1;

    uint32_t expected_new_cnt_req = old_cnt_req+1;
    while( *preg_cnt_req != expected_new_cnt_req){
      std::chrono::steady_clock::now()>tp_timeout;
      isTimeout = true;
      break;
    }

    if(isTimeout){
      std::printf("fail: timeout during waiting of CNT_REQ++, after request #%zu\n", n);
      break;
    }

    std::this_thread::sleep_for(10us);
  }

  uint32_t new_cnt_req = *preg_cnt_req;
  if(new_cnt_req-old_cnt_req != numReq){
    std::printf("fail: %zu requests are sent, but CNT_REQ+=%u\n",
		numReq, new_cnt_req-old_cnt_req);
  }

  uint32_t new_cnt_trig = *preg_cnt_trig;
  if(new_cnt_trig-old_cnt_trig != new_cnt_req-old_cnt_req){
    std::printf("warning: CNT_REQ+=%u, but CNT_TRIG+=%u\n",
		new_cnt_req-old_cnt_req, new_cnt_trig-old_cnt_trig);
  }
}



void test_trigger_speed_max(){
  std::this_thread::sleep_for(20ms);
  std::printf("\n\nchecking sequence trigger max speed\n");
  size_t numMaxReq = 10000000;
  size_t numReq = 0;
  bool isTimeout = false;
  uint32_t old_cnt_req = *preg_cnt_req;
  uint32_t old_cnt_trig = *preg_cnt_trig;
  
  // uint32_t begin_trig_id = *preg_trig_id;
  uint32_t begin_trig_id = 0;
  auto tp_start = std::chrono::high_resolution_clock::now();
  auto tp_timeout = tp_start + std::chrono::seconds(10); 
  for(size_t n = 0; n < numMaxReq && std::chrono::high_resolution_clock::now()<tp_timeout; n++ ){
    // increase trigger id  +1
    *preg_trig_id = begin_trig_id + n;
    
    numReq = n;
    while((*preg_req_wait & 1) != 1){
      std::chrono::high_resolution_clock::now()>tp_timeout;
      isTimeout = true;
      break;
    }
    if(isTimeout){
      std::printf("fail: timeout during waiting of REQ_WAIT==1, before request #%zu\n", n);
      std::printf("fail: unable to send all %zu requests. only %zu requests are sent.\n",
		  numMaxReq, numReq);
      break;
    }

    //new req
    uint32_t old_cnt_req = *preg_cnt_req;
    uint32_t old_cnt_trig = *preg_cnt_trig;
    *preg_req = 1;
    numReq = n+1;
    std::this_thread::sleep_for(1ns);

    uint32_t expected_new_cnt_req = old_cnt_req+1;
    while( *preg_cnt_req != expected_new_cnt_req){
      std::chrono::high_resolution_clock::now()>tp_timeout;
      isTimeout = true;
      break;
    }
    if(isTimeout){
      std::printf("fail: timeout during waiting of CNT_REQ++, after request #%zu\n", n);
      break;
    }
    /*
    uint32_t expected_new_cnt_trig = old_cnt_trig+1;
    while( *preg_cnt_trig != expected_new_cnt_trig){
      std::chrono::high_resolution_clock::now()>tp_timeout;
      isTimeout = true;
      break;
    }
    if(isTimeout){
      std::printf("fail: timeout during waiting of CNT_TRIG++, after request #%zu\n", n);
      break;
    }
    */
  }

  uint32_t new_cnt_req = *preg_cnt_req;
  if(new_cnt_req-old_cnt_req != numReq){
    std::printf("fail: %zu requests are sent, but CNT_REQ+=%u\n",
		numReq, new_cnt_req-old_cnt_req);
  }

  uint32_t new_cnt_trig = *preg_cnt_trig;
  if(new_cnt_trig-old_cnt_trig != new_cnt_req-old_cnt_req){
    std::printf("warning: CNT_REQ+=%u, but CNT_TRIG+=%u\n",
		new_cnt_req-old_cnt_req, new_cnt_trig-old_cnt_trig);
  }

  std::printf("info: CNT_REQ+=%u, CNT_TRIG+=%u\n",
	      new_cnt_req-old_cnt_req, new_cnt_trig-old_cnt_trig);
  
}


int main(int argc, char** argv){
  if(argc!=2){
    std::fprintf(stdout, "%s\n", help_usage.c_str());
    std::exit(-1);
  }

  off_t addrbase_phy; 
  if(std::regex_match(argv[1], std::regex("\\s*(?:(0[Xx])?([0-9a-fA-F]+))\\s*")) ){
    std::cmatch mt;
    std::regex_match(argv[1], mt, std::regex("\\s*(?:(0[Xx])?([0-9a-fA-F]+))\\s*"));
    addrbase_phy = std::stoull(mt[2].str(), 0, mt[1].str().empty()?10:16);
  }
  else{
    std::fprintf(stderr, "%s is not a valid address\n", argv[1]);
    std::fprintf(stdout, "%s\n", help_usage.c_str());
    std::exit(-1);
  }
  
  int fd;
  if ((fd = open("/dev/mem", O_RDWR|O_SYNC)) < 0 ) {
    std::fprintf(stderr, "Error opening file. \n");
    std::exit(-1);
  }

  size_t page_size = getpagesize();  
  off_t offset_in_page = addrbase_phy & (page_size - 1);
  size_t mapped_size = page_size * ( (offset_in_page + SIZE_ADDR_MAP)/page_size + 1);
  void *map_base = mmap(NULL,
			mapped_size,
			PROT_READ | PROT_WRITE,
			MAP_SHARED,
			fd,
			addrbase_phy & ~(off_t)(page_size - 1));

  if (map_base == MAP_FAILED){
    std::fprintf(stderr, "Memory mapped failed\n");
    std::exit(-1);
  }
  char* pchar_base = (char*)map_base + offset_in_page;

  preg_req_wait =  reinterpret_cast<uint32_t *>(pchar_base+ADDROFF_REQ_WAIT);
  preg_req      =  reinterpret_cast<uint32_t *>(pchar_base+ADDROFF_REQ);
  preg_trig_id  =  reinterpret_cast<uint32_t *>(pchar_base+ADDROFF_TRIG_ID);
  preg_cnt_req  =  reinterpret_cast<uint32_t *>(pchar_base+ADDROFF_CNT_REQ);
  preg_cnt_trig =  reinterpret_cast<uint32_t *>(pchar_base+ADDROFF_CNT_TRIG);

  // test_trig_id_read_write();
  // test_req_wait_when_idle();
  // test_cnt_req();
  // test_cnt_trig();

  // test_seq_id_trig();  
  test_trigger_speed_max();
  
  close(fd);
  return 1;
}
