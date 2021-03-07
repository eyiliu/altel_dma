#include <cstdio>
#include <cstring>
#include <cstdlib>

#include <iostream>

#include <signal.h>

#include "FirmwarePortal.hh"
#include "Telescope.hh"

#include "getopt.h"
#include "linenoise.h"

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


static const std::string reg_json_default=
#include "altel_reg_cmd_list_json.hh"
;

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
  A) init  (set firmware and sensosr to ready-run state after power-cirlce or reset)
   > start

  B) start (set firmware and sensosr to running state from ready-run state)
   > start

  C) stop  (set firmware and sensosr to stop-run state)
   > stop

  D) reset (reset firmware)
   > reset

  E) print regiesters and command list)
   > info regcmd

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
  std::string file_context = reg_json_default;

  FirmwarePortal fw(file_context, "");
  FirmwarePortal *m_fw = &fw;

  const char* linenoise_history_path = "/tmp/.alpide_cmd_history";
  linenoiseHistoryLoad(linenoise_history_path);
  linenoiseSetCompletionCallback([](const char* prefix, linenoiseCompletions* lc)
                                 {
                                   static const char* examples[] =
                                     {"help", "info", "selftrigger", "start", "stop", "init", "reset", "regcmd",
                                      "quit", "sensor", "firmware", "set", "get",
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
      break;
    }
    if ( std::regex_match(result, std::regex("\\s*(quit)\\s*")) ){
      printf("quiting \n");
      linenoiseHistoryAdd(result);
      free(result);
      break;
    }

    if ( std::regex_match(result, std::regex("\\s*(help)\\s*")) ){
      fprintf(stdout, "%s", help_usage_linenoise.c_str());
    }
    else if ( std::regex_match(result, std::regex("\\s*(reset)\\s*")) ){
      printf("reset \n");
      m_fw->SendFirmwareCommand("RESET");
      m_fw->SetFirmwareRegister("FIRMWARE_RESET", 0xff);
    }
    else if ( std::regex_match(result, std::regex("\\s*(init)\\s*")) ){
      printf("init \n");
      // begin init
      //  m_fw->SendFirmwareCommand("RESET");
      m_fw->SetFirmwareRegister("TRIG_DELAY", 1); //25ns per dig (FrameDuration?)
      // m_fw->SetFirmwareRegister("GAP_INT_TRIG", 20);

      //=========== init part ========================
      // 3.8 Chip initialization
      // GRST
      m_fw->SetFirmwareRegister("FIRMWARE_MODE", 0);
      m_fw->SetFirmwareRegister("ADDR_CHIP_ID", 0x10); //OB
      m_fw->SendAlpideBroadcast("GRST"); // chip global reset
      m_fw->SetAlpideRegister("CHIP_MODE", 0x3c); // configure mode
      // DAC setup
      m_fw->SetAlpideRegister("VRESETP", 0x75); //117
      m_fw->SetAlpideRegister("VRESETD", 0x93); //147
      m_fw->SetAlpideRegister("VCASP", 0x56);   //86
      uint32_t vcasn = 57;
      uint32_t ithr  = 51;
      m_fw->SetAlpideRegister("VCASN", vcasn);   //57 Y50
      m_fw->SetAlpideRegister("VPULSEH", 0xff); //255
      m_fw->SetAlpideRegister("VPULSEL", 0x0);  //0
      m_fw->SetAlpideRegister("VCASN2",vcasn+12);  //62 Y63  VCASN+12
      m_fw->SetAlpideRegister("VCLIP", 0x0);    //0
      m_fw->SetAlpideRegister("VTEMP", 0x0);
      m_fw->SetAlpideRegister("IAUX2", 0x0);
      m_fw->SetAlpideRegister("IRESET", 0x32);  //50
      m_fw->SetAlpideRegister("IDB", 0x40);     //64
      m_fw->SetAlpideRegister("IBIAS", 0x40);   //64
      m_fw->SetAlpideRegister("ITHR", ithr);   //51  empty 0x32; 0x12 data, not full.  0x33 default, threshold
      // 3.8.1 Configuration of in-pixel logic
      m_fw->SendAlpideBroadcast("PRST");  //pixel matrix reset
      m_fw->SetPixelRegisterFullChip("MASK_EN", 0);
      m_fw->SetPixelRegisterFullChip("PULSE_EN", 0);
      m_fw->SendAlpideBroadcast("PRST");  //pixel matrix reset
      // 3.8.2 Configuration and start-up of the Data Transmission Unit, PLL
      m_fw->SetAlpideRegister("DTU_CONF", 0x008d); // default
      m_fw->SetAlpideRegister("DTU_DAC",  0x0088); // default
      m_fw->SetAlpideRegister("DTU_CONF", 0x0085); // clear pll disable bit
      m_fw->SetAlpideRegister("DTU_CONF", 0x0185); // set pll reset bit
      m_fw->SetAlpideRegister("DTU_CONF", 0x0085); // clear reset bit
      // 3.8.3 Setting up of readout
      // 3.8.3.1a (OB) Setting CMU and DMU Configuration Register
      m_fw->SetAlpideRegister("CMU_DMU_CONF", 0x70); //Token, disable MCH, enable DDR, no previous OB
      m_fw->SetAlpideRegister("TEST_CTRL", 0x400); //Disable Busy Line
      // 3.8.3.2 Setting FROMU Configuration Registers and enabling readout mode
      // FROMU Configuration Register 1,2
      m_fw->SetAlpideRegister("FROMU_CONF_1", 0x00); //Disable external busy, no triger delay
      m_fw->SetAlpideRegister("FROMU_CONF_2", 20); //STROBE duration, alice testbeam 100
      // FROMU Pulsing Register 1,2
      // m_fw->SetAlpideRegister("FROMU_PULSING_2", 0xffff); //yiliu: test pulse duration, max
      // Periphery Control Register (CHIP MODE)
      // m_fw->SetAlpideRegister("CHIP_MODE", 0x3d); //trigger MODE
      // RORST
      // m_fw->SendAlpideBroadcast("RORST"); //Readout (RRU/TRU/DMU) reset, commit token
      //===========end of init part =====================

      //user init
      //
      //
      m_fw->SetFirmwareRegister("DEVICE_ID", 0xff);
      //
      //end of user init
      std::fprintf(stdout, " fw init  %s\n", m_fw->DeviceUrl().c_str());
    }
    else if ( std::regex_match(result, std::regex("\\s*(selftrigger)\\s+(set)\\s+(?:(0[Xx])?([0-9]+))\\s*")) ){
      std::cmatch mt;
      std::regex_match(result, mt, std::regex("\\s*(selftrigger)\\s+(set)\\s+(?:(0[Xx])?([0-9]+))\\s*"));
      uint64_t freq = std::stoull(mt[4].str(), 0, mt[3].str().empty()?10:16);
      uint64_t cir_per_trigger = 40000000/freq;
      std::fprintf(stdout, "set GAP_INT_TRIG = %#llx  %llu\n", cir_per_trigger, cir_per_trigger); 
      fw.SetFirmwareRegister("GAP_INT_TRIG", cir_per_trigger);
    }
    else if ( std::regex_match(result, std::regex("\\s*(selftrigger)\\s+(get)\\s*")) ){
      std::cmatch mt;
      std::regex_match(result, mt, std::regex("\\s*(selftrigger)\\s+(get)\\s*"));
      uint64_t cir_per_trigger  = fw.GetFirmwareRegister("GAP_INT_TRIG");
      uint64_t freq = 40000000/cir_per_trigger;
      fprintf(stderr, "cir_per_trigger = %#llx   %llu \n", cir_per_trigger, cir_per_trigger);
      fprintf(stderr, "freq = %#llx   %llu \n", freq, freq);
    }
    else if ( std::regex_match(result, std::regex("\\s*(start)\\s*")) ){
      printf("starting ext trigger, mode 1\n");
      m_fw->SetAlpideRegister("CMU_DMU_CONF", 0x70);// token
      m_fw->SetAlpideRegister("CHIP_MODE", 0x3d); //trigger MODE
      m_fw->SendAlpideBroadcast("RORST"); //Readout (RRU/TRU/DMU) reset, commit token
      // m_fw->SetFirmwareRegister("FIRMWARE_STOP", 0x00);//remove stop flag
      m_fw->SetFirmwareRegister("FIRMWARE_MODE", 1); //run inter trigger
      std::fprintf(stdout, " fw start %s\n", m_fw->DeviceUrl().c_str());
    }
    else if ( std::regex_match(result, std::regex("\\s*(stop)\\s*")) ){
      printf("stopping\n");
      m_fw->SetFirmwareRegister("FIRMWARE_MODE", 0); //fw must be stopped before chip
      // m_fw->SetFirmwareRegister("FIRMWARE_STOP", 0xff); // set stop flag
      m_fw->SetAlpideRegister("CHIP_MODE", 0x3c); // configure mode
      std::fprintf(stdout, " fw stop  %s\n", m_fw->DeviceUrl().c_str());
    }
    else if ( std::regex_match(result, std::regex("\\s*(info)\\s+(regcmd)\\s*"))){
      std::cout<< file_context<<std::endl;
    }
    else if ( std::regex_match(result, std::regex("\\s*(sensor)\\s+(set)\\s+(\\w+)\\s+(?:(0[Xx])?([0-9]+))\\s*")) ){
      std::cmatch mt;
      std::regex_match(result, mt, std::regex("\\s*(sensor)\\s+(set)\\s+(\\w+)\\s+(?:(0[Xx])?([0-9]+))\\s*"));
      std::string name = mt[3].str();
      uint64_t value = std::stoull(mt[5].str(), 0, mt[4].str().empty()?10:16);
      fw.SetAlpideRegister(name, value);
    }
    else if ( std::regex_match(result, std::regex("\\s*(sensor)\\s+(get)\\s+(\\w+)\\s*")) ){
      std::cmatch mt;
      std::regex_match(result, mt, std::regex("\\s*(sensor)\\s+(get)\\s+(\\w+)\\s*"));
      std::string name = mt[3].str();
      uint64_t value = fw.GetAlpideRegister(name);
      fprintf(stderr, "%s = %llu, %#llx\n", name.c_str(), value, value);
    }
    else if ( std::regex_match(result, std::regex("\\s*(firmware)\\s+(set)\\s+(\\w+)\\s+(?:(0[Xx])?([0-9]+))\\s*")) ){
      std::cmatch mt;
      std::regex_match(result, mt, std::regex("\\s*(firmware)\\s+(set)\\s+(\\w+)\\s+(?:(0[Xx])?([0-9]+))\\s*"));
      std::string name = mt[3].str();
      uint64_t value = std::stoull(mt[5].str(), 0, mt[4].str().empty()?10:16);
      fw.SetFirmwareRegister(name, value);
    }
    else if ( std::regex_match(result, std::regex("\\s*(firmware)\\s+(get)\\s+(\\w+)\\s*")) ){
      std::cmatch mt;
      std::regex_match(result, mt, std::regex("\\s*(firmware)\\s+(get)\\s+(\\w+)\\s*"));
      std::string name = mt[3].str();
      uint64_t value = fw.GetFirmwareRegister(name);
      fprintf(stderr, "%s = %llu, %#llx\n", name.c_str(), value, value);
    }
    linenoiseHistoryAdd(result);
    free(result);
  }

  linenoiseHistorySave(linenoise_history_path);
  linenoiseHistoryFree();
}
