#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <iostream>
#include <signal.h>

#include "AltelReader.hh"
#include "getopt.h"

namespace{
  std::string CStringToHexString(const char *bin, int len){
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

  std::string StringToHexString(const std::string bin){
    return CStringToHexString(bin.data(), bin.size());
  }
}


static const std::string help_usage = R"(
Usage:
  -help                        help message
  -verbose                     verbose flag
  -ipAddress      <ip>         eg. 192.168.10.16

  -rawPrint                    print data by hex format in terminal
  -rawFile        <path>       path of raw file to save

examples:
#1. save data and print
./tcprx_dump -i 192.168.10.16 -rawPrint -rawFile test.dat

#2. save data only
./tcprx_dump -i 192.168.10.16 -rawFile test.dat

#3. print only
./tcprx_dump -i 192.168.10.16 -rawPrint
)";

static sig_atomic_t g_done = 0;
int main(int argc, char *argv[]) {
  signal(SIGINT, [](int){g_done+=1;});

  std::string rawFilePath;
  std::string ipAddressStr;
  bool do_rawPrint = false;

  int do_verbose = 0;
  {////////////getopt begin//////////////////
    struct option longopts[] = {{"help",      no_argument, NULL, 'h'},//option -W is reserved by getopt
                                {"verbose",   no_argument, NULL, 'v'},//val
                                {"ipAddress", required_argument, NULL, 'm'},
                                {"rawPrint",  no_argument, NULL, 's'},
                                {"rawFile",   required_argument, NULL, 'f'},
                                {0, 0, 0, 0}};

    if(argc == 1){
      std::fprintf(stderr, "%s\n", help_usage.c_str());
      std::exit(1);
    }
    int c;
    int longindex;
    opterr = 1;
    while ((c = getopt_long_only(argc, argv, "-", longopts, &longindex)) != -1) {
      // // "-" prevents non-option argv
      // if(!optopt && c!=0 && c!=1 && c!=':' && c!='?'){ //for debug
      //   std::fprintf(stdout, "opt:%s,\targ:%s\n", longopts[longindex].name, optarg);;
      // }
      switch (c) {
      case 'm':
        ipAddressStr = optarg;
        break;
      case 'f':
        rawFilePath = optarg;
        break;
      case 's':
        do_rawPrint = true;
        break;
        // help and verbose
      case 'v':
        do_verbose=1;
        //option is set to no_argument
        if(optind < argc && *argv[optind] != '-'){
          do_verbose = std::stoul(argv[optind]);
          optind++;
        }
        break;
      case 'h':
        std::fprintf(stdout, "%s\n", help_usage.c_str());
        std::exit(0);
        break;
        /////generic part below///////////
      case 0:
        // getopt returns 0 for not-NULL flag option, just keep going
        break;
      case 1:
        // If the first character of optstring is '-', then each nonoption
        // argv-element is handled as if it were the argument of an option
        // with character code 1.
        std::fprintf(stderr, "%s: unexpected non-option argument %s\n",
                     argv[0], optarg);
        std::exit(1);
        break;
      case ':':
        // If getopt() encounters an option with a missing argument, then
        // the return value depends on the first character in optstring:
        // if it is ':', then ':' is returned; otherwise '?' is returned.
        std::fprintf(stderr, "%s: missing argument for option %s\n",
                     argv[0], longopts[longindex].name);
        std::exit(1);
        break;
      case '?':
        // Internal error message is set to print when opterr is nonzero (default)
        std::exit(1);
        break;
      default:
        std::fprintf(stderr, "%s: missing getopt branch %c for option %s\n",
                     argv[0], c, longopts[longindex].name);
        std::exit(1);
        break;
      }
    }
  }/////////getopt end////////////////


  std::fprintf(stdout, "\n");
  std::fprintf(stdout, "ipAddress: %s\n", ipAddressStr.c_str());
  std::fprintf(stdout, "rawPrint:  %d\n", do_rawPrint);
  std::fprintf(stdout, "rawFile:   %s\n", rawFilePath.c_str());
  std::fprintf(stdout, "\n");

  if(ipAddressStr.empty()){
    std::fprintf(stderr, "ERROR: ip address is not provided.\n\n");
    std::fprintf(stderr, "%s\n", help_usage.c_str());
  }

  if (rawFilePath.empty() && !do_rawPrint) {
    std::fprintf(stderr, "ERROR: nether rawPrint or rawFile is set.\n\n");
    std::fprintf(stderr, "%s\n", help_usage.c_str());
    std::exit(1);
  }


  static const std::string rd_conf_str = R"(
  {
    "protocol":"tcp",
    "version": 1,
    "description":"ALPIDE plane TCP raw reader",
    "options":{
      "ip":"192.168.10.16",
      "port":24
    }
  }
)";

  std::FILE *fp = nullptr;
  if(!rawFilePath.empty()){
    std::filesystem::path filepath(rawFilePath);
    std::filesystem::path path_dir_output = std::filesystem::absolute(filepath).parent_path();
    std::filesystem::file_status st_dir_output =
      std::filesystem::status(path_dir_output);
    if (!std::filesystem::exists(st_dir_output)) {
      std::fprintf(stdout, "Output folder does not exist: %s\n\n",
                   path_dir_output.c_str());
      std::filesystem::file_status st_parent =
        std::filesystem::status(path_dir_output.parent_path());
      if (std::filesystem::exists(st_parent) &&
          std::filesystem::is_directory(st_parent)) {
        if (std::filesystem::create_directory(path_dir_output)) {
          std::fprintf(stdout, "Create output folder: %s\n\n", path_dir_output.c_str());
        } else {
          std::fprintf(stderr, "Unable to create folder: %s\n\n", path_dir_output.c_str());
          throw;
        }
      } else {
        std::fprintf(stderr, "Unable to create folder: %s\n\n", path_dir_output.c_str());
        throw;
      }
    }

    std::filesystem::file_status st_file = std::filesystem::status(filepath);
    if (std::filesystem::exists(st_file)) {
      std::fprintf(stderr, "File < %s > exists.\n\n", filepath.c_str());
      throw;
    }

    fp = std::fopen(filepath.c_str(), "w");
    if (!fp) {
      std::fprintf(stderr, "File opening failed: %s \n\n", filepath.c_str());
      throw;
    }
  }

  auto js_rd_conf = JsonUtils::createJsonDocument(rd_conf_str);
  js_rd_conf["options"]["ip"].SetString(ipAddressStr.data(), ipAddressStr.size(), js_rd_conf.GetAllocator());
  std::unique_ptr<AltelReader> rd(new AltelReader(JsonUtils::stringJsonValue(js_rd_conf)));
  std::fprintf(stdout, " connecting to %s\n", rd->DeviceUrl().c_str());
  if(!rd->Open()){
      std::fprintf(stdout, " connection fail\n");
      throw;
  }
  std::fprintf(stdout, " connected\n");


  size_t dataFrameN = 0;
  while(!g_done){
    auto df = rd->Read(std::chrono::seconds(1));
    if(!df){
      std::fprintf(stdout, "Data reveving timeout\n");
      std::fprintf(stdout, "!!!!!you might need to start alpide with rbcp tool (rbcp_main) \n");
      std::fprintf(stdout, "!!!!!you might need to trigger alpide \n");
      continue;
    }
    if(do_rawPrint){
      std::fprintf(stdout, "\nDataFrame #%d,  TLU #%d\n", dataFrameN, df->GetCounter());
      std::fprintf(stdout, "RawData_TCP_RX:\n%s\n", StringToHexString(df->m_raw).c_str());
    }

    if(fp){
      std::fwrite(df->m_raw.data(), 1, df->m_raw.size(), fp);
      std::fflush(fp);
    }
    dataFrameN ++;
  }
  rd->Close();
  if(fp){
    std::fflush(fp);
    std::fclose(fp);
  }
  return 0;
}
