#include <cstdio>
#include <cctype>
#include <cstdlib>
#include <cstring>

#include <iostream>
#include <fstream>

#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "myrapidjson.h"
#include "AltelReader.hh"


#define HEADER_BYTE  (0x5a)
#define FOOTER_BYTE  (0xa5)


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


AltelReader::~AltelReader(){
  Close();
}

AltelReader::AltelReader(const rapidjson::GenericValue<rapidjson::UTF8<>, rapidjson::CrtAllocator> &js){
  m_js_conf.CopyFrom<rapidjson::CrtAllocator>(js , m_jsa);

  m_fd = 0;
  m_flag_file = false;
  auto& js_proto = m_js_conf["protocol"];
  auto& js_opt = m_js_conf["options"];
  if(js_proto != "file"){
    std::fprintf(stderr, "ERROR<%s>: Unknown reader protocol: %s\n", __func__, js_proto.GetString());
    throw;
  }
  m_file_path = js_opt["path"].GetString();
  m_file_terminate_eof = js_opt["terminate_eof"].GetBool();
  m_flag_file = true;

};

const std::string& AltelReader::DeviceUrl(){
  return m_tcp_ip;
}

bool AltelReader::Open(){
  m_fd = open(m_file_path.c_str(), O_RDONLY | O_NONBLOCK);
  if(!m_fd)
    return false;
  return true;
}

void AltelReader::Close(){
  if(!m_fd)
    return;

  close(m_fd);
  m_fd = 0;
}



std::vector<DataFrameSP> AltelReader::Read(size_t size_max_pkg,
                                           const std::chrono::milliseconds &timeout_idel,
                                           const std::chrono::milliseconds &timeout_total){
  std::chrono::system_clock::time_point tp_timeout_total = std::chrono::system_clock::now() + timeout_total;
  std::vector<DataFrameSP> pkg_v;
  while(1){
    DataFrameSP pkg = Read(timeout_idel);
    if(pkg){
      pkg_v.push_back(pkg);
      if(pkg_v.size()>=size_max_pkg){
        break;
      }
    }
    else{
      break; 
    }
    if(std::chrono::system_clock::now() > tp_timeout_total){
      break;
    }
  }
  return pkg_v;
}

DataFrameSP AltelReader::Read(const std::chrono::milliseconds &timeout_idel){ //timeout_read_interval
  size_t size_buf_min = 12;
  size_t size_buf = size_buf_min;
  std::string buf(size_buf, 0);
  size_t size_filled = 0;
  std::chrono::system_clock::time_point tp_timeout_idel;
  bool can_time_out = false;
  int read_len_real = 0;
  while(size_filled < size_buf){
    read_len_real = read(m_fd, &buf[size_filled], size_buf-size_filled);
    if(read_len_real>0){
      std::fprintf(stdout, ">>>read  %d Bytes \n", read_len_real); 
    }
    if (read_len_real < 0) {
      if (errno == EAGAIN)
	continue; //no problem, just no data
      fprintf(stderr, "ERROR on reading from axidmard, errno=%d\n", errno);
      std::fprintf(stderr, "ERROR<%s@%s>: read(...) returns error code %i\n", __func__, m_tcp_ip.c_str(), read_len_real);
      throw;
    }
      
    if(read_len_real== 0){
      if(!can_time_out){
	can_time_out = true;
	tp_timeout_idel = std::chrono::system_clock::now() + timeout_idel;
      }
      else{
	if(std::chrono::system_clock::now() > tp_timeout_idel){
	  // std::cerr<<"JadeRead: reading timeout\n";
	  if(size_filled == 0){
	    if(m_file_terminate_eof)
	      return nullptr;
	    else{
	      std::fprintf(stdout, "INFO<%s@%s>: no data receving.\n",  __func__, m_tcp_ip.c_str());
	    }
	    return nullptr;
	  }
	  //TODO: keep remain data, nothrow
	  std::fprintf(stderr, "ERROR<%s@%s>: error of incomplete data reading \n", __func__ , m_tcp_ip.c_str());
	  throw;
	}
      }
      continue;
    }

    size_filled += read_len_real;
    can_time_out = false;
    // std::cout<<" size_buf size_buf_min  size_filled<< size_buf << " "<< size_buf_min<<" " << size_filled<<std::endl;
    if(size_buf == size_buf_min  && size_filled >= size_buf_min){
      uint8_t header_byte =  buf.front();

      uint32_t w1 = *reinterpret_cast<const uint32_t*>(buf.data()+4);
      // uint8_t rsv = (w1>>20) & 0xf;

      uint32_t size_payload = (w1 & 0xfffff);
      // std::cout<<" size_payload "<< size_payload<<std::endl;
      if(header_byte != HEADER_BYTE){
        std::fprintf(stderr, "ERROR<%s@%s>: wrong header of data frame\n", __func__, m_tcp_ip.c_str());

        //TODO: skip brocken data
        throw;
      }

      size_buf += (size_payload + 4) & -4 ;
      
      buf.resize(size_buf);
    }
  }

  uint32_t w_end = *reinterpret_cast<const uint32_t*>(&buf.back()-3);

  // uint8_t footer_byte =  buf.back();
  if(w_end != FOOTER_BYTE && (w_end>>8)!= FOOTER_BYTE && (w_end>>16)!= FOOTER_BYTE && (w_end>>24)!= FOOTER_BYTE ){
    std::fprintf(stderr, "ERROR<%s@%s>:  wrong footer of data frame\n", __func__,  m_tcp_ip.c_str());
    std::fprintf(stderr, "dumpping data Hex:\n%s\n", StringToHexString(buf).c_str());
    //TODO: skip broken data. do not know what happenned to broken data
    throw;
  }
  // std::fprintf(stdout, "dumpping data Hex:\n%s\n", StringToHexString(buf).c_str());

  auto df = std::make_shared<DataFrame>();
  df->m_raw=std::move(buf);
  return df;
  // return std::make_shared<DataFrame>(std::move(buf));
}


DataFrameSP AltelReader::ReadRaw(size_t len, const std::chrono::milliseconds &timeout_idel){ //timeout_read_interval

  size_t size_buf_min = 0;
  size_t size_buf = (len+3)/4*4;
  std::string buf(size_buf, 0);
  size_t size_filled = 0;
  std::chrono::system_clock::time_point tp_timeout_idel;
  bool can_time_out = false;
  while(size_filled < size_buf){
    int read_len_real = read(m_fd, &buf[size_filled], size_buf-size_filled);
    if(read_len_real== 0 || ((read_len_real < 0) && (errno == EAGAIN))){//no data
      if(!can_time_out){
	can_time_out = true;
	tp_timeout_idel = std::chrono::system_clock::now() + timeout_idel;
      }
      else{
	if(std::chrono::system_clock::now() > tp_timeout_idel){
	  if(size_filled == 0){
	    std::fprintf(stdout, "INFO<%s@%s>: no data receving.\n",  __func__, m_tcp_ip.c_str());
	    return nullptr;
	  }
	  else{
	    //TODO: keep remain data, nothrow
	    std::fprintf(stderr, "ERROR<%s@%s>: error of incomplete data reading \n", __func__ , m_tcp_ip.c_str());
	    throw;
	  }
	}
	continue;
      }
    }
    else if(read_len_real < 0 && errno != EAGAIN) {
      fprintf(stderr, "ERROR on reading from axidmard, errno=%d\n", errno);
      std::fprintf(stderr, "ERROR<%s@%s>: read(...) returns error code %i\n", __func__, m_tcp_ip.c_str(), read_len_real);
      throw;
    }
    else{//read_len_real>0 , has data
      // std::fprintf(stdout, "read  %d Bytes \n", read_len_real);
      // std::fprintf(stdout, "Hex:   \n%s\n", StringToHexString(buf).c_str());
      if(read_len_real%4){
	std::fprintf(stderr, "read incomplete word, %d bytes \n", read_len_real);
	throw;
      }
      size_filled += read_len_real;
      can_time_out = false;
    }
    //back to loop
  }
  // std::fprintf(stdout, "dumpping data Hex:\n%s\n", StringToHexString(buf).c_str());

  // uint32_t *p32_buf = reinterpret_cast<uint32_t*>(&buf[0]);
  // for(size_t n = 0; n<size_filled/4;){
  //   *p32_buf = BE32TOH(*p32_buf);
  //   n++;
  //   p32_buf ++;
  // }

  auto df = std::make_shared<DataFrame>();
  df->m_raw=std::move(buf);
  return df;
}

std::string AltelReader::LoadFileToString(const std::string& path){
  std::ifstream ifs(path);
  if(!ifs.good()){
    std::fprintf(stderr, "ERROR<%s>: unable to load file<%s>\n", __func__, path.c_str());
    throw;
  }
  std::string str;
  str.assign((std::istreambuf_iterator<char>(ifs) ),
             (std::istreambuf_iterator<char>()));
  return str;
}
