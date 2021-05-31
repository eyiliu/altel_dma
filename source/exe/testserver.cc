
#include "TcpServer.hh"
#include <iostream>
#include <csignal>


static sig_atomic_t g_done = 0;

std::vector<std::vector<std::pair<uint16_t,uint16_t>>>  hots= {
  {
    // 0
    {550,329},
    {500,103},
    {617,58},
    {736,460},
    {330,465},
    {967,376},
    {471,194},
    {214,362},
    {908,427},
    {632,508},
    {14,410},
    {166,393},
    {694,376},
    {404,319},
    {664,309},
    {408,226},
    {812,178},
    {202,147},
    {17,105},
    {0,54},
    {323,51},
    {126,48},
    {768,15},
    {747,23},
    {952,40},
    {1017,74},
    {698,77},
    {11,116},
    {745,123},
    {895,145},
    {754,187},
    {153,163},
    {951,171},
    {899,191},
    {954,236},
    {44,248},
    {31,250},
    {381,253},
    {750,333},
    {117,394},
    {132,399},
    {567,451},
    {308,467},
    {843,56},
    {562,219},
    {891,292},
    {478,14},
    {450,247},
    {890,54},
    {243,212},
    {733,60},
    {991,255}
  },

  // 1
  {

  },

  // 2
  {
    {264,509},
    {960,466},
    {51,212},
    {1020,67},
    {689,349},
    {512,299},
    {966,255},
    {831,19},
    {925,19},
    {509,92},
    {394,242},
    {373,254},
    {646,270},
    {39,291},
    {326,339},
    {440,419}
  },
 // 3
  {

  },

  // 4
  {

  },

  // 5
  {

  },

  // 6
  {

  }
};



int main(int argn, char ** argc){
  int n = 0;
  if(argn != 1){
    n = std::stoi(std::string(argc[1]));
    n = n%6;
  }
  std::cout<< "set device id: "<<n<<std::endl;

  signal(SIGINT, [](int){g_done+=1;});
  std::unique_ptr<TcpServer> tcpServer(new TcpServer(9000, n, hots[n]));
  while(!g_done){
    std::this_thread::sleep_for (std::chrono::seconds(1));
  }
  return 0;
}
