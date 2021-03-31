#include "Telescope.hh"

#include <regex>

//using namespace std::chrono_literals;
using namespace altel;

namespace{
  std::string TimeNowString(const std::string& format){
    std::time_t time_now = std::time(nullptr);
    std::string str_buffer(100, char(0));
    size_t n = std::strftime(&str_buffer[0], sizeof(str_buffer.size()),
                             format.c_str(), std::localtime(&time_now));
    str_buffer.resize(n?(n-1):0);
    return str_buffer;
  }
}

Telescope::Telescope(const std::string& file_context){
  rapidjson::GenericDocument<rapidjson::UTF8<char>, rapidjson::CrtAllocator>  js_doc;
  js_doc.Parse(file_context);

  if(js_doc.HasParseError()){
    fprintf(stderr, "JSON parse error: %s (at string position %lu) \n", rapidjson::GetParseError_En(js_doc.GetParseError()), js_doc.GetErrorOffset());
    throw;
  }
  const auto &js_obj = js_doc.GetObject();
  
  if(!js_obj.HasMember("telescope")){
    fprintf(stderr, "JSON configure file error: no \"telescope\" section \n");
    throw;
  }
  
  if(!js_obj.HasMember("testbeam")){
    fprintf(stderr, "JSON configure file error: no \"testbeam\" section \n");
    throw;
  }

  if(!js_obj.HasMember("layers")){
    fprintf(stderr, "JSON configure file error: no \"layers\" section \n");
    throw;
  }
  
  const auto& js_telescope  = js_obj["telescope"];
  const auto& js_testbeam   = js_obj["testbeam"];
  const auto& js_layers     = js_obj["layers"];

  m_js_telescope.CopyFrom<rapidjson::CrtAllocator>(js_telescope, m_jsa);
  m_js_testbeam.CopyFrom<rapidjson::CrtAllocator>(js_testbeam, m_jsa);
  
  std::map<std::string, double> layer_loc;
  std::multimap<double, std::string> loc_layer;
 
  if(!js_telescope.HasMember("locations")){
    fprintf(stderr, "JSON configure file error: no \"location\" section \n");
    throw;
  }
  
  // throw;
  for(const auto& l: js_telescope["locations"].GetObject()){
    std::string name = l.name.GetString();
    double loc = l.value.GetDouble();
    layer_loc[name] = loc;
    loc_layer.insert(std::pair<double, std::string>(loc, name));
  }

  if(!js_testbeam.HasMember("energy")){
    fprintf(stderr, "JSON configure file error: no energy \n");
    throw;
  }
  
  
  for(const auto& l: loc_layer){
    std::string layer_name = l.second;
    bool layer_found = false;
    for (const auto& js_layer : js_layers.GetArray()){
      if(js_layer.HasMember("name") && js_layer["name"]==layer_name){
        std::unique_ptr<Layer> l(new Layer);
        l->m_fw.reset(new FirmwarePortal(FirmwarePortal::Stringify(js_layer["ctrl_link"])));
        l->m_rd.reset(new AltelReader(FirmwarePortal::Stringify(js_layer["data_link"])));
        l->m_name=layer_name;
        m_vec_layer.push_back(std::move(l));
        layer_found = true;
        break;
      }
    }
    if(!layer_found){
      std::fprintf(stderr, "Layer %6s: is not found in configure file \n", layer_name.c_str());
      throw;
    }
    std::fprintf(stdout, "Layer %6s:     at location Z = %8.2f\n", layer_name.c_str(), l.first);
  }

  double energy=js_testbeam["energy"].GetDouble();
  std::fprintf(stdout, "Testbeam energy:  %.1f\n", energy);
  
  if(!m_js_telescope.HasMember("config")){
      std::fprintf(stderr, "JSON configure file error: no telescope config \n");
      throw;
  }
  
  const auto& js_tele_conf = m_js_telescope["config"];
  for(auto &l: m_vec_layer){
    std::string name = l->m_name;
    if(!js_tele_conf.HasMember(name)){
      std::fprintf(stderr, "JSON configure file error: no config %s \n", name.c_str());
      throw;
    }
    l->m_js_conf.CopyFrom(js_tele_conf[name], l->m_jsa);
  }

}

Telescope::~Telescope(){
  Stop();
}

std::vector<DataFrameSP> Telescope::ReadEvent(){
  std::vector<DataFrameSP> ev_sync;
  if (!m_is_running) return ev_sync;  
  
  uint32_t trigger_n = -1;
  for(auto &l: m_vec_layer){
    if( l->Size() == 0){
      // TODO check cached size of all layers
      return ev_sync;
    }
    else{
      uint32_t trigger_n_ev = l->Front()->GetTrigger();
      if(trigger_n_ev< trigger_n)
        trigger_n = trigger_n_ev;
    }
  }

  for(auto &l: m_vec_layer){
    auto &ev_front = l->Front(); 
    if(ev_front->GetTrigger() == trigger_n){
      ev_sync.push_back(ev_front);
      l->PopFront();
    }
  }
  
  if(ev_sync.size() < m_vec_layer.size() ){
    std::cout<< "dropped assambed event with subevent less than requried "<< m_vec_layer.size() <<" sub events" <<std::endl;
    std::string dev_numbers;
    for(auto & ev : ev_sync){
      dev_numbers += std::to_string(ev->GetExtension());
      dev_numbers +=" ";
    }
    std::cout<< "  tluID="<<trigger_n<<" subevent= "<< dev_numbers <<std::endl;
    std::vector<DataFrameSP> empty;
    return empty;
  }
  if(m_mon_ev_read == m_mon_ev_write){
    m_ev_last=ev_sync;
    m_mon_ev_write ++;
  }
  m_st_n_ev ++;
  return ev_sync;
}

std::vector<DataFrameSP> Telescope::ReadEvent_Lastcopy(){
  if(m_mon_ev_write > m_mon_ev_read){
    std::vector<DataFrameSP> re_ev_last = m_ev_last;
    m_mon_ev_read ++;
    return re_ev_last;
  }
  else
    return m_ev_last_empty;
}

void Telescope::Init(){
  for(auto & l: m_vec_layer){
    l->fw_init();
  }
  for(auto & l: m_vec_layer){
    l->fw_conf();
  }
}

void Telescope::Start(){
  m_st_n_ev = 0;
  m_mon_ev_read = 0;
  m_mon_ev_write = 0;
  // for(auto & l: m_vec_layer){
  //   l->fw_conf();
  // }

  for(auto & l: m_vec_layer){
    l->rd_start();
  }

  for(auto & l: m_vec_layer){
    l->fw_start();
  }
  std::fprintf(stdout, "tel_start \n");

  if(!m_is_async_watching){
    m_fut_async_watch = std::async(std::launch::async, &Telescope::AsyncWatchDog, this);
  }

  m_fut_async_rd = std::async(std::launch::async, &Telescope::AsyncRead, this);
  m_is_running = true;
}

void Telescope::Start_no_tel_reading(){ // TO be removed,
  m_st_n_ev = 0;
  m_mon_ev_read = 0;
  m_mon_ev_write = 0;

  for(auto & l: m_vec_layer){
    l->rd_start();
  }

  for(auto & l: m_vec_layer){
    l->fw_start();
  }

  if(!m_is_async_watching){
    m_fut_async_watch = std::async(std::launch::async, &Telescope::AsyncWatchDog, this);
  }
  //m_fut_async_rd = std::async(std::launch::async, &Telescope::AsyncRead, this);
  m_is_running = true;
}

void Telescope::Stop(){
  m_is_async_reading = false;
  if(m_fut_async_rd.valid())
    m_fut_async_rd.get();

  m_is_async_watching = false;
  if(m_fut_async_watch.valid())
    m_fut_async_watch.get();
  
  for(auto & l: m_vec_layer){
    l->fw_stop();
  }
  
  for(auto & l: m_vec_layer){
    l->rd_stop();
  }

  for(auto & l: m_vec_layer){
    l->ClearBuffer();
  }
  
  m_is_running = false;
}

uint64_t Telescope::AsyncRead(){
  auto now = std::chrono::system_clock::now();
  auto now_c = std::chrono::system_clock::to_time_t(now);
  std::string now_str = TimeNowString("%y%m%d%H%M%S");
  std::string data_path = "data/alpide_"+now_str+".json";
  FILE* fd = fopen(data_path.c_str(), "wb");
  rapidjson::StringBuffer js_sb;
  rapidjson::Writer<rapidjson::StringBuffer> js_writer;
  js_writer.SetMaxDecimalPlaces(5);
  uint64_t n_ev = 0;
  m_flag_next_event_add_conf = true;
  m_is_async_reading = true;
  while (m_is_async_reading){
    auto ev = ReadEvent();
    if(ev.empty()){
      std::this_thread::sleep_for(std::chrono::microseconds(100));
      continue;
    }

    n_ev ++;
    //continue;
    js_sb.Clear();
    if(n_ev == 1){
      std::fwrite(reinterpret_cast<const char *>("[\n"), 1, 2, fd);
    }
    else{
      std::fwrite(reinterpret_cast<const char *>(",\n"), 1, 2, fd);
    }
    
    js_writer.Reset(js_sb);
    js_writer.StartObject();
    if(m_flag_next_event_add_conf){
      rapidjson::PutN(js_sb, '\n', 1);
      js_writer.String("testbeam");
      m_js_testbeam.Accept(js_writer);
      js_writer.String("telescope");
      m_js_telescope.Accept(js_writer);
      m_flag_next_event_add_conf = false;
    }
    if(m_count_st_js_write > m_count_st_js_read){
      rapidjson::PutN(js_sb, '\n', 1);
      js_writer.String("status");
      m_js_status.Accept(js_writer);
      m_count_st_js_read ++;
    }
    
    rapidjson::PutN(js_sb, '\n', 1);
    js_writer.String("layers");
    js_writer.StartArray();
    for(auto& e: ev){
      auto js_e = e->JSON(m_jsa);
      js_e.Accept(js_writer);
      rapidjson::PutN(js_sb, '\n', 1);
    }
    js_writer.EndArray();
    js_writer.EndObject();
    rapidjson::PutN(js_sb, '\n', 1);
    auto p_ch = js_sb.GetString();
    auto n_ch = js_sb.GetSize();
    std::fwrite(reinterpret_cast<const char *>(p_ch), 1, n_ch, fd);
  }
  
  std::fwrite(reinterpret_cast<const char *>("]"), 1, 2, fd);
  fclose(fd);
  std::fprintf(stdout, "Tele: disk file closed\n");
  std::fprintf(stdout,"- %s  %lu Events\n", data_path.c_str(), n_ev); 
  return n_ev;
}

uint64_t Telescope::AsyncWatchDog(){
  m_is_async_watching = true;
  while(m_is_async_watching){
    rapidjson::GenericValue<rapidjson::UTF8<char>, rapidjson::CrtAllocator> js_status(rapidjson::kObjectType);
    std::this_thread::sleep_for(std::chrono::seconds(1));
    for(auto &l: m_vec_layer){
      std::string l_status = l->GetStatusString();
      std::fprintf(stdout, "%s\n", l_status.c_str());
      
      rapidjson::GenericValue<rapidjson::UTF8<char>, rapidjson::CrtAllocator> name;
      rapidjson::GenericValue<rapidjson::UTF8<char>, rapidjson::CrtAllocator> value;
      name.SetString(l->m_name, m_jsa);
      value.SetString(l_status, m_jsa);
      js_status.AddMember(std::move(name), std::move(value), m_jsa);      
    }
    uint64_t st_n_ev = m_st_n_ev;
    std::fprintf(stdout, "Tele: disk saved events(%lu) \n\n", st_n_ev);
    m_flag_next_event_add_conf = true;

    //TODO: make a json object to keep status;
    if(m_count_st_js_read == m_count_st_js_write){
      std::string now_str = TimeNowString("%Y-%m-%d %H:%M:%S");
      js_status.AddMember("time", std::move(now_str), m_jsa);
      m_js_status = std::move(js_status);
      m_count_st_js_write ++;      
    }
  }
  //sleep and watch running time status;
  return 0;
}
