#include "DataFrame.hh"
#include "mysystem.hh"
#include <iostream>

using namespace altel;

#ifndef DEBUG_PRINT
#define DEBUG_PRINT 0
#endif
#define debug_print(fmt, ...)                                           \
  do { if (DEBUG_PRINT) std::fprintf(stdout, fmt, ##__VA_ARGS__); } while (0)

#ifndef INFO_PRINT
#define INFO_PRINT 0
#endif
#define info_print(fmt, ...)                                           \
  do { if (INFO_PRINT) std::fprintf(stdout, fmt, ##__VA_ARGS__); } while (0)



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


void DataFrame::fromRaw(const std::string &raw){
    const uint8_t* p_raw_beg = reinterpret_cast<const uint8_t *>(raw.data());
    const uint8_t* p_raw = p_raw_beg;
    if(raw.size()<16){
      std::fprintf(stderr, "raw data length is less than 16\n");
      throw;
    }
    if( *p_raw_beg!=0x5a){
      std::fprintf(stderr, "package header/trailer mismatch, head<%hhu>\n", *p_raw_beg);
      throw;
    }

    p_raw++; //header   
    p_raw++; //resv
    p_raw++; //resv

    uint8_t deviceId = *p_raw;
    m_extension=*p_raw;

    debug_print(">>deviceId %hhu\n", deviceId);
    p_raw++; //deviceId

    uint32_t len_payload_data = *reinterpret_cast<const uint32_t*>(p_raw) & 0x00ffffff;
    uint32_t len_pack_expected = (len_payload_data + 16) & -4;
    if( len_pack_expected  != raw.size()){
      std::fprintf(stderr, "raw data length does not match to package size\n");
      std::fprintf(stderr, "payload_len = %u,  package_size = %zu\n",
                   len_payload_data, raw.size());
      throw;
    }
    p_raw += 4;

    uint32_t triggerId = *reinterpret_cast<const uint16_t*>(p_raw);
    debug_print(">>triggerId %u\n", triggerId);
    m_counter = *reinterpret_cast<const uint16_t*>(p_raw);
    m_trigger = m_counter;
  
    p_raw += 4;

    const uint8_t* p_payload_end = p_raw_beg + 12 + len_payload_data -1;
    if( *(p_payload_end+1) != 0xa5 ){
      std::fprintf(stderr, "package header/trailer mismatch, trailer<%hu>\n", *(p_payload_end+1) );
      throw;
    }
    
    ClusterPool pool;
    uint8_t l_frame_n = -1;
    uint8_t l_region_id = -1;
    while(p_raw <= p_payload_end){
      char d = *p_raw;
      if(d & 0b10000000){
        debug_print("//1     NOT DATA\n");
        if(d & 0b01000000){
          debug_print("//11    EMPTY or REGION HEADER or BUSY_ON/OFF\n");
          if(d & 0b00100000){
            debug_print("//111   EMPTY or BUSY_ON/OFF\n");
            if(d & 0b00010000){
              debug_print("//1111  BUSY_ON/OFF\n");
              p_raw++;
              continue;
            }
            debug_print("//1110  EMPTY\n");
            uint8_t chip_id = d & 0b00001111;
            l_frame_n++;
            p_raw++;
            d = *p_raw;
            uint8_t bunch_counter_h = d;
            p_raw++;
            continue;
          }
          debug_print("//110   REGION HEADER\n");
          l_region_id = d & 0b00011111;
          debug_print(">>region_id %hhu\n", l_region_id);
          p_raw++;
          continue;
        }
        debug_print("//10    CHIP_HEADER/TRAILER or UNDEFINED\n");
        if(d & 0b00100000){
          debug_print("//101   CHIP_HEADER/TRAILER\n");
          if(d & 0b00010000){
            debug_print("//1011  TRAILER\n");
            uint8_t readout_flag= d & 0b00001111;
            p_raw++;
            continue;
          }
          debug_print("//1010  HEADER\n");
          uint8_t chip_id = d & 0b00001111;
          l_frame_n++;
          p_raw++;
          d = *p_raw;
          uint8_t bunch_counter_h = d;
          p_raw++;
          continue;
        }
        debug_print("//100   UNDEFINED\n");
        p_raw++;
        continue;
      }
      else{
        debug_print("//0     DATA\n");
        if(d & 0b01000000){
          debug_print("//01    DATA SHORT\n"); // 2 bytes
          uint8_t encoder_id = (d & 0b00111100)>> 2;
          uint16_t addr = (d & 0b00000011)<<8;
          p_raw++;
          d = *p_raw;
          addr += *p_raw;
          p_raw++;

          uint16_t y = addr>>1;
          uint16_t x = (l_region_id<<5)+(encoder_id<<1)+((addr&0b1)!=((addr>>1)&0b1));
          debug_print("[%hu, %hu, %hhu]\n", x, y, deviceId);
          continue;
        }
        debug_print("//00    DATA LONG\n"); // 3 bytes
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
        debug_print("[%hu, %hu, %hhu] ", x, y, deviceId);
	pool.addHit(x, y, m_extension);

	
        for(int i=1; i<=7; i++){
          if(hit_map & (1<<(i-1))){
            uint16_t addr_l = addr + i;
            uint16_t y = addr_l>>1;
            uint16_t x = (l_region_id<<5)+(encoder_id<<1)+((addr_l&0b1)!=((addr_l>>1)&0b1));
            debug_print("[%hu, %hu, %hhu] ", x, y, deviceId);
	    pool.addHit(x, y, m_extension);

          }
        }
        debug_print("\n");
        continue;
      }
    }



    pool.buildClusters();
    m_clusters = std::move(pool.m_clusters);

    return;
}




void DataFrame::Print(std::ostream& os, size_t ws) const
{  
  rapidjson::OStreamWrapper osw(os);
  rapidjson::Writer<rapidjson::OStreamWrapper> writer(osw);
  rapidjson::CrtAllocator allo;
  JSON(allo).Accept(writer);
}
