#include "DataFrame.hh"
#include "mysystem.hh"
#include <iostream>

using namespace altel;

DataFrame::DataFrame(std::string&& raw)
  : m_raw(std::move(raw))
{
  fromRaw(m_raw);
}

DataFrame::DataFrame(const std::string& raw)
  : m_raw(raw)
{
  fromRaw(m_raw);
}

DataFrame::DataFrame(const rapidjson::Value &js){
  fromJSON<>(js);
}

DataFrame::DataFrame(const rapidjson::GenericValue<
                     rapidjson::UTF8<char>,
                     rapidjson::CrtAllocator> &js){
  fromJSON<rapidjson::CrtAllocator>(js);
}

void DataFrame::fromRaw(const std::string &raw, uint32_t level){
  if(level <= m_level_decode)
    return;
  const uint8_t* p_raw_beg = reinterpret_cast<const uint8_t *>(raw.data());
  const uint8_t* p_raw_end = p_raw_beg + raw.size();
  const uint8_t* p_raw = p_raw_beg;
  if(raw.size()<8){
    std::cerr << "JadeDataFrame: raw data length is less than 8\n";
    throw;
  }
  // std::cout << JadeUtils::ToHexString(raw)<<std::endl;
  if( *p_raw_beg!=0x5a || *(p_raw_end-1)!=0xa5){
    std::cerr << "JadeDataFrame: pkg header/trailer mismatch\n";
    //std::cerr << JadeUtils::ToHexString(raw)<<std::endl;
    std::cerr <<uint16_t((*p_raw_beg))<<std::endl;
    std::cerr <<uint16_t((*(p_raw_end-1)))<<std::endl;
    throw;
  }
  p_raw++;
  m_extension=*p_raw;
  uint32_t len_payload_data = BE32TOH(*reinterpret_cast<const uint32_t*>(p_raw)) & 0x000fffff;
  if (len_payload_data + 8 != raw.size()) {
    std::cerr << "JadeDataFrame: raw data length does not match\n";
    std::cerr << len_payload_data<<std::endl;
    std::cerr << raw.size()<<std::endl;
    throw;
  }
  p_raw += 4;
  m_counter = BE16TOH(*reinterpret_cast<const uint16_t*>(p_raw));
  m_trigger = m_counter;
  p_raw += 2;
  // m_n_x = 1024;
  // m_n_y = 512;

  if(level<2){
    m_level_decode = level;
    return;
  }

  ClusterPool pool;
  
  uint8_t l_frame_n = -1;
  uint8_t l_region_id = -1;
  while(p_raw < p_raw_end-1){
    char d = *p_raw;
    // std::cout << JadeUtils::ToHexString(&d, 1)<<std::endl;    
    if(d & 0b10000000){
      // std::cout<<"//NOT DATA 1"<<std::endl;
      if(d & 0b01000000){
        // std::cout<<"//empty or region header or busy_on/off 11"<<std::endl;
        if(d & 0b00100000){
          // std::cout<<"//emtpy or busy_on/off 111"<<std::endl;
          if(d & 0b00010000){
            // std::cout<<"//busy_on/off"<<std::endl;
            p_raw++;
            continue;
          }
          // std::cout<<"// empty 1110"<<std::endl;
          uint8_t chip_id = d & 0b00001111;
          l_frame_n++;
          p_raw++;
          d = *p_raw;
          uint8_t bunch_counter_h = d;
          p_raw++;
          continue;
        }
        // std::cout<<"// region header 110"<<std::endl;
        l_region_id = d & 0b00011111;
        p_raw++;
        continue;
      }
      // std::cout<<"//CHIP_HEADER/TRAILER or undefined 10"<<std::endl;
      if(d & 0b00100000){
        // std::cout<<"//CHIP_HEADER/TRAILER 101"<<std::endl;
        if(d & 0b00010000){
          // std::cout<<"//TRAILER 1011"<<std::endl;
          uint8_t readout_flag= d & 0b00001111;
          p_raw++;
          continue;
        }
        // std::cout<<"//HEADER 1010"<<std::endl;
        uint8_t chip_id = d & 0b00001111;
        l_frame_n++;
        p_raw++;
        d = *p_raw;
        uint8_t bunch_counter_h = d;
        p_raw++;
        continue;
      }
      std::cout<<"//undefined 100"<<std::endl;
      p_raw++;
      continue;
    }
    else{
      // std::cout<<"//DATA 0"<<std::endl;
      if(d & 0b01000000){
        // std::cout<<"//DATA SHORT 01"<<std::endl;
        if(level>2){
          uint8_t encoder_id = (d & 0b00111100)>> 2;
          uint16_t addr = (d & 0b00000011)<<8;
          p_raw++;
          d = *p_raw;
          addr += *p_raw;
          p_raw++;

          uint16_t y = addr>>1;
          uint16_t x = (l_region_id<<5)+(encoder_id<<1)+((addr&0b1)!=((addr>>1)&0b1));

          pool.addHit(x, y, m_extension);
          // m_data_x.push_back(x);
          // m_data_y.push_back(y);
          // m_data_d.push_back(m_extension);
        }
        else{
          p_raw++;
          p_raw++;
        }
        continue;
      }
      // std::cout<<"//DATA LONG 00"<<std::endl;
      if(level>2){
        uint8_t encoder_id = (d & 0b00111100)>> 2;
        uint16_t addr = (d & 0b00000011)<<8;
        p_raw++;
        d = *p_raw;
        addr += *p_raw;
        p_raw++;
        d = *p_raw;
        uint8_t hit_map = (d & 0b01111111);
        p_raw++;
        uint16_t y = addr>>1;
        uint16_t x = (l_region_id<<5)+(encoder_id<<1)+((addr&0b1)!=((addr>>1)&0b1));
 
        // m_data_x.push_back(x);
        // m_data_y.push_back(y);
        // m_data_d.push_back(m_extension);
        pool.addHit(x, y, m_extension);

        for(int i=1; i<=7; i++){
          if(hit_map & (1<<(i-1))){
	    uint16_t addr_l = addr + i;
            uint16_t y = addr_l>>1;
            uint16_t x = (l_region_id<<5)+(encoder_id<<1)+((addr_l&0b1)!=((addr_l>>1)&0b1));
            // m_data_x.push_back(x);
            // m_data_y.push_back(y);
            // m_data_d.push_back(m_extension);
            pool.addHit(x, y, m_extension);
          }
        }
      }
      else{
        p_raw++;
        p_raw++;
        p_raw++;
      }
      continue;
    }
  }
  
  pool.buildClusters();
  m_clusters = std::move(pool.m_clusters);
  
  // m_n_d = l_frame_n+1;
  m_level_decode = level;
  return;
}

void DataFrame::Print(std::ostream& os, size_t ws) const
{  
  rapidjson::OStreamWrapper osw(os);
  rapidjson::Writer<rapidjson::OStreamWrapper> writer(osw);
  rapidjson::CrtAllocator allo;
  JSON(allo).Accept(writer);
}
