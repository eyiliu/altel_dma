
#include <regex>
#include "Layer.hh"

//using namespace std::chrono_literals;
using namespace altel;

Layer::~Layer(){

  m_is_async_reading = false;
  if(m_fut_async_rd.valid())
    m_fut_async_rd.get();

  m_is_async_watching = false;
  if(m_fut_async_watch.valid())
    m_fut_async_watch.get();

}


void Layer::fw_start(){
  if(!m_fw) return;
  m_fw->SetAlpideRegister("CMU_DMU_CONF", 0x70);// token
  m_fw->SetAlpideRegister("CHIP_MODE", 0x3d); //trigger MODE
  m_fw->SendAlpideBroadcast("RORST"); //Readout (RRU/TRU/DMU) reset, commit token
  m_fw->SetFirmwareRegister("FIRMWARE_MODE", 1); //run ext trigger
  std::fprintf(stdout, " fw start %s\n", m_fw->DeviceUrl().c_str());

}



void Layer::fw_stop(){
  if(!m_fw) return;
  m_fw->SetFirmwareRegister("FIRMWARE_MODE", 0); //fw must be stopped before chip
  m_fw->SetAlpideRegister("CHIP_MODE", 0x3c); // configure mode
  std::fprintf(stdout, " fw stop  %s\n", m_fw->DeviceUrl().c_str());
}

void Layer::fw_conf(){
  if(!m_fw) return;
  std::fprintf(stdout, " fw conf %s\n", m_fw->DeviceUrl().c_str());

  if(!m_js_conf.HasMember("hotmask")){
    fprintf(stderr, "JSON configure file error: no hotmask section \n");
    throw;
  }
  const auto& js_hotmask =  m_js_conf["hotmask"];
  // std::printf("\nMasking Layer %s ", m_name.c_str());
  for(const auto &hot: js_hotmask.GetArray()){
    uint64_t  x = hot[0].GetUint64();
    uint64_t  y = hot[1].GetUint64();
    // std::printf(" [%u, %u] ", x, y);
    m_fw->SetPixelRegister(x, y, "MASK_EN", true);
  }
  // std::printf("\n ");

  if(!m_js_conf.HasMember("firmware")){
    fprintf(stderr, "JSON configure file error: no firmware section \n");
    throw;
  }
  const auto& js_fw_conf =  m_js_conf["firmware"];
  for(const auto &reg: js_fw_conf.GetObject()){
    m_fw->SetFirmwareRegister(reg.name.GetString(), reg.value.GetUint64());
  }

  if(!m_js_conf.HasMember("sensor")){
    fprintf(stderr, "JSON configure file error: no sensor section \n");
    throw;
  }
  const auto& js_sn_conf =  m_js_conf["sensor"];
  for(const auto &reg: js_sn_conf.GetObject()){
    m_fw->SetAlpideRegister(reg.name.GetString(), reg.value.GetUint64());
  }

}

void Layer::fw_init(){
  if(!m_fw) return;

  //  m_fw->SendFirmwareCommand("RESET");
  m_fw->SetFirmwareRegister("TRIG_DELAY", 1); //25ns per dig (FrameDuration?)
  m_fw->SetFirmwareRegister("GAP_INT_TRIG", 20);

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

  //
  //end of user init

  std::fprintf(stdout, " fw init  %s\n", m_fw->DeviceUrl().c_str());
}

void Layer::rd_start(){
  if(m_is_async_reading){
    std::cout<< "old AsyncReading() has not been stopped "<<std::endl;
    return;
  }

  m_fut_async_rd = std::async(std::launch::async, &Layer::AsyncPushBack, this);
  if(!m_is_async_watching){
    m_fut_async_watch = std::async(std::launch::async, &Layer::AsyncWatchDog, this);
  }
}

void Layer::rd_stop(){
  m_is_async_reading = false;
  if(m_fut_async_rd.valid())
    m_fut_async_rd.get();

  m_is_async_watching = false;
  if(m_fut_async_watch.valid())
    m_fut_async_watch.get();
}

uint64_t Layer::AsyncPushBack(){ // IMPROVE IT AS A RING
  m_vec_ring_ev.clear();
  m_vec_ring_ev.resize(m_size_ring);
  m_count_ring_write = 0;
  m_count_ring_read = 0;
  m_hot_p_read = m_size_ring -1; // tail

  uint32_t tg_expected = 0;
  uint32_t flag_wait_first_event = true;

  m_rd->Open();
  std::fprintf(stdout, " rd start  %s\n", m_rd->DeviceUrl().c_str());
  m_is_async_reading = true;

  m_st_n_tg_ev_now =0;
  m_st_n_ev_input_now =0;
  m_st_n_ev_output_now =0;
  m_st_n_ev_bad_now =0;
  m_st_n_ev_overflow_now =0;
  m_st_n_tg_ev_begin = 0;
  
  std::chrono::system_clock::time_point tp_reset_fw = std::chrono::system_clock::now() + std::chrono::seconds(10);
  while (m_is_async_reading){
    auto df = m_rd? m_rd->Read(std::chrono::seconds(1)):nullptr; // TODO: read a vector
    if(!df){
      if(!m_st_n_ev_input_now && std::chrono::system_clock::now() > tp_reset_fw){
	//reset fw, init conf, reopen rd
	std::cout<<"\n===================reset fw============================\n"<<std::endl;
	m_fw->SendFirmwareCommand("RESET");
	m_rd->Close();
	std::this_thread::sleep_for(std::chrono::seconds(3));
	fw_init();
	fw_conf();
	m_rd->Open();
	fw_start();
	std::cout<<"===================finish reset fw============================\n"<<std::endl;	
	tp_reset_fw = std::chrono::system_clock::now() + std::chrono::seconds(10);
      }
      continue;
    }
    m_st_n_ev_input_now ++;

    uint64_t next_p_ring_write = m_count_ring_write % m_size_ring;
    if(next_p_ring_write == m_hot_p_read){
      // buffer full, permanent data lose
      m_st_n_ev_overflow_now ++;
      continue;
    }

    uint16_t tg_l16 = 0xffff & df->GetCounter();
    //std::cout<< "id "<< tg_l16 <<"  ";
    if(flag_wait_first_event){
      flag_wait_first_event = false;
      m_extension = df->GetExtension() ;
      tg_expected = tg_l16;
      m_st_n_tg_ev_begin = tg_expected;
    }
    if(tg_l16 != (tg_expected & 0xffff)){
      // std::cout<<(tg_expected & 0x7fff)<< " " << tg_l16<<"\n";
      uint32_t tg_guess_0 = (tg_expected & 0xffff0000) + tg_l16;
      uint32_t tg_guess_1 = (tg_expected & 0xffff0000) + 0x10000 + tg_l16;
      if(tg_guess_0 > tg_expected && tg_guess_0 - tg_expected < 200){
        // std::cout<< "missing trigger, expecting : provided "<< (tg_expected & 0xffff) << " : "<< tg_l16<<" ("<< m_extension <<") \n";
        tg_expected =tg_guess_0;
      }
      else if (tg_guess_1 > tg_expected && tg_guess_1 - tg_expected < 200){
        // std::cout<< "missing trigger, expecting : provided "<< (tg_expected & 0xffff) << " : "<< tg_l16<<" ("<< m_extension <<") \n";
        tg_expected =tg_guess_1;
      }
      else{
        // std::cout<< "broken trigger ID, expecting : provided "<< (tg_expected & 0xffff) << " : "<< tg_l16<<" ("<<df->GetExtension() <<") \n";
        tg_expected ++;
        m_st_n_ev_bad_now ++;
        // permanent data lose
        continue;
      }
    }
    //TODO: fix tlu firmware, mismatch between modes AIDA start at 1, EUDET start at 0
    df->SetTrigger(tg_expected); 
    m_st_n_tg_ev_now = tg_expected;

    m_vec_ring_ev[next_p_ring_write] = df;
    m_count_ring_write ++;
    tg_expected ++;
  }
  m_rd->Close();
  std::fprintf(stdout, " rd stop  %s\n", m_rd->DeviceUrl().c_str());
  return m_count_ring_write;
}

uint64_t Layer::AsyncWatchDog(){
  m_tp_run_begin = std::chrono::system_clock::now();
  m_tp_old = m_tp_run_begin;
  m_is_async_watching = true;

  m_st_n_tg_ev_old =0;
  m_st_n_ev_input_old = 0;
  m_st_n_ev_bad_old =0;
  m_st_n_ev_overflow_old = 0;

  
  while(m_is_async_watching){
    std::this_thread::sleep_for(std::chrono::seconds(1));
    uint64_t st_n_tg_ev_begin = m_st_n_tg_ev_begin;
    uint64_t st_n_tg_ev_now = m_st_n_tg_ev_now;
    uint64_t st_n_ev_input_now = m_st_n_ev_input_now;
    //uint64_t st_n_ev_output_now = m_st_n_ev_output_now;
    uint64_t st_n_ev_bad_now = m_st_n_ev_bad_now;
    uint64_t st_n_ev_overflow_now = m_st_n_ev_overflow_now;

    // time
    auto tp_now = std::chrono::system_clock::now();
    std::chrono::duration<double> dur_period_sec = tp_now - m_tp_old;
    std::chrono::duration<double> dur_accu_sec = tp_now - m_tp_run_begin;
    double sec_period = dur_period_sec.count();
    double sec_accu = dur_accu_sec.count();

    //std::cout<< "sec "<< sec_period<< " : " << sec_accu<<std::endl;

    // period
    uint64_t st_n_tg_ev_period = st_n_tg_ev_now - m_st_n_tg_ev_old;
    uint64_t st_n_ev_input_period = st_n_ev_input_now - m_st_n_ev_input_old;
    uint64_t st_n_ev_bad_period = st_n_ev_bad_now - m_st_n_ev_bad_old;
    uint64_t st_n_ev_overflow_period = st_n_ev_overflow_now - m_st_n_ev_overflow_old;
    
    // ratio
    //double st_output_vs_input_accu = st_n_ev_input_now? st_ev_output_now / st_ev_input_now : 1;
    double st_bad_vs_input_accu = st_n_ev_input_now? 1.0 * st_n_ev_bad_now / st_n_ev_input_now : 0;
    double st_overflow_vs_input_accu = st_n_ev_input_now? 1.0 *  st_n_ev_overflow_now / st_n_ev_input_now : 0;
    double st_input_vs_trigger_accu = st_n_ev_input_now? 1.0 * st_n_ev_input_now / (st_n_tg_ev_now - st_n_tg_ev_begin + 1) : 1;
    
    //double st_output_vs_input_period = st_ev_input_period? st_ev_output_period / st_ev_input_period : 1;
    double st_bad_vs_input_period = st_n_ev_input_period? 1.0 * st_n_ev_bad_period / st_n_ev_input_period : 0;
    double st_overflow_vs_input_period = st_n_ev_input_period? 1.0 *  st_n_ev_overflow_period / st_n_ev_input_period : 0;
    double st_input_vs_trigger_period = st_n_tg_ev_period? 1.0 *  st_n_ev_input_period / st_n_tg_ev_period : 1;
    
    // hz
    double st_hz_tg_accu = (st_n_tg_ev_now - st_n_tg_ev_begin + 1) / sec_accu ;
    double st_hz_input_accu = st_n_ev_input_now / sec_accu ; 

    double st_hz_tg_period = st_n_tg_ev_period / sec_period ;
    double st_hz_input_period = st_n_ev_input_period / sec_period ;

    std::string st_string_new =
      FirmwarePortal::FormatString("L<%u> event(%d)/trigger(%d - %d)=Ev/Tr(%.4f) dEv/dTr(%.4f) tr_accu(%.2f hz) ev_accu(%.2f hz) tr_period(%.2f hz) ev_period(%.2f hz)",
                                   m_extension, st_n_ev_input_now, st_n_tg_ev_now, st_n_tg_ev_begin, st_input_vs_trigger_accu, st_input_vs_trigger_period,
                                   st_hz_tg_accu, st_hz_input_accu, st_hz_tg_period, st_hz_input_period
                                   );
    
    {
      std::unique_lock<std::mutex> lk(m_mtx_st);
      m_st_string = std::move(st_string_new);
    }
    
    //write to old
    m_st_n_tg_ev_old = st_n_tg_ev_now;
    m_st_n_ev_input_old = st_n_ev_input_now;
    m_st_n_ev_bad_old = st_n_ev_bad_now;
    m_st_n_ev_overflow_old = st_n_ev_overflow_now;
    m_tp_old = tp_now;
  }
  return 0;
}

std::string  Layer::GetStatusString(){
  std::unique_lock<std::mutex> lk(m_mtx_st);
  return m_st_string;
}

DataFrameSP& Layer::Front(){
  if(m_count_ring_write > m_count_ring_read) {
    uint64_t next_p_ring_read = m_count_ring_read % m_size_ring;
    m_hot_p_read = next_p_ring_read;
    // keep hot read to prevent write-overlapping
    return m_vec_ring_ev[next_p_ring_read];
  }
  else{
    return m_ring_end;
  }
}

void Layer::PopFront(){
  if(m_count_ring_write > m_count_ring_read) {
    uint64_t next_p_ring_read = m_count_ring_read % m_size_ring;
    m_hot_p_read = next_p_ring_read;
    // keep hot read to prevent write-overlapping
    m_vec_ring_ev[next_p_ring_read].reset();
    m_count_ring_read ++;
  }
}

uint64_t Layer::Size(){
  return  m_count_ring_write - m_count_ring_read;
}

void Layer::ClearBuffer(){
  m_count_ring_write = m_count_ring_read;
  m_vec_ring_ev.clear();
}
