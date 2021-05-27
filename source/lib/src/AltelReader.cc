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
  std::string CStringToHexString(const char *bin, const int len){
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

  std::string StringToHexString(const std::string& bin){
    return CStringToHexString(bin.data(), bin.size());
  }





}


  size_t dumpBrokenData(int fd_rx){
    size_t len_total = 0;
    uint32_t buf_word;
    char * p_buf = reinterpret_cast<char*>(&buf_word);

    while(read(fd_rx, p_buf, 4)>0){
      len_total ++;
      if(buf_word == 0xa5 || buf_word>>8 == 0xa5 || buf_word>>16 == 0xa5 || buf_word>>24 == 0xa5){
	// reach pack end
	return len_total;
      }
    }
    return len_total;
  }


AltelReader::~AltelReader(){
  Close();
}

AltelReader::AltelReader(){
  // m_js_conf.CopyFrom<rapidjson::CrtAllocator>(js , m_jsa);

  m_fd = 0;
  // auto& js_proto = m_js_conf["protocol"];
  // auto& js_opt = m_js_conf["options"];
  // m_file_path = js_opt["path"].GetString();

  m_file_path = "/dev/axidmard"; 
};

const std::string& AltelReader::DeviceUrl(){
  return m_file_path;
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
                                           const std::chrono::milliseconds &timeout_idle,
                                           const std::chrono::milliseconds &timeout_total){
  std::chrono::system_clock::time_point tp_timeout_total = std::chrono::system_clock::now() + timeout_total;
  std::vector<DataFrameSP> pkg_v;
  while(1){
    DataFrameSP pkg = Read(timeout_idle);
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


std::string readPack(int fd_rx, const std::chrono::milliseconds &timeout_idel){ //timeout_read_interval
  // std::fprintf(stderr, "-");
  size_t size_buf_min = 16;
  size_t size_buf = size_buf_min;
  std::string buf(size_buf, 0);
  size_t size_filled = 0;
  std::chrono::system_clock::time_point tp_timeout_idel;
  bool can_time_out = false;
  int read_len_real = 0;
  while(size_filled < size_buf){
    read_len_real = read(fd_rx, &buf[size_filled], size_buf-size_filled);
    if(read_len_real>0){
      // debug_print(">>>read  %d Bytes \n", read_len_real);
      size_filled += read_len_real;
      can_time_out = false;
      if(size_buf == size_buf_min  && size_filled >= size_buf_min){
	uint8_t header_byte =  buf.front();
	uint32_t w1 = *reinterpret_cast<const uint32_t*>(buf.data()+4);
	// uint8_t rsv = (w1>>20) & 0xf;

	uint32_t size_payload = (w1 & 0xfffff);
	// std::cout<<" size_payload "<< size_payload<<std::endl;
	if(header_byte != HEADER_BYTE){
	  std::fprintf(stderr, "ERROR<%s>: wrong header of data frame, skip\n", __func__);
	  std::fprintf(stderr, "RawData_TCP_RX:\n%s\n", StringToHexString(buf).c_str());
	  std::fprintf(stderr, "<");
	  //TODO: skip broken data
	  dumpBrokenData(fd_rx);
	  size_buf = size_buf_min;
	  size_filled = 0;
	  can_time_out = false;
	  continue;
	}
	size_buf += size_payload;
	size_buf &= -4; // aligment 32bits, tail 32bits might be cutted.
	if(size_buf > 300){
	  size_buf = 300;
	}
	buf.resize(size_buf);
      }
    }
    else if (read_len_real== 0 || (read_len_real < 0 && errno == EAGAIN)){ // empty readback, read again
      if(!can_time_out){
	can_time_out = true;
	tp_timeout_idel = std::chrono::system_clock::now() + timeout_idel;
      }
      else{
	if(std::chrono::system_clock::now() > tp_timeout_idel){
	  if(size_filled == 0){
	    // debug_print("INFO<%s>: no data receving.\n",  __func__);
	    return std::string();
	  }
	  //TODO: keep remain data, nothrow
	  std::fprintf(stderr, "ERROR<%s>: timeout error of incomplete data reading \n", __func__ );
	  std::fprintf(stderr, "=");
	  return std::string();
	  // throw;
	}
      }
      continue;
    }
    else{
      std::fprintf(stderr, "ERROR<%s>: read(...) returns error code %d\n", __func__,  errno);
      throw;
    }
  }
  uint32_t w_end = *reinterpret_cast<const uint32_t*>(&buf.back()-3);

  if(w_end != FOOTER_BYTE && (w_end>>8)!= FOOTER_BYTE && (w_end>>16)!= FOOTER_BYTE && (w_end>>24)!= FOOTER_BYTE ){
    std::fprintf(stderr, "ERROR<%s>:  wrong footer of data frame\n", __func__);
    std::fprintf(stderr, ">");
    std::fprintf(stderr, "dumpping data Hex:\n%s\n", StringToHexString(buf).c_str());
    return std::string();
    //throw;
  }
  // std::fprintf(stdout, "dumpping data Hex:\n%s\n", StringToHexString(buf).c_str());
  return buf;
}

DataFrameSP AltelReader::Read(const std::chrono::milliseconds &timeout_idle){ //timeout_read_interval
  auto buf = readPack(m_fd, timeout_idle);
  if(buf.empty()){
    return nullptr;
  }
  // auto df = std::make_shared<DataFrame>();
  // df->m_raw=std::move(buf);
  // return df;
  return std::make_shared<DataFrame>(std::move(buf));
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

std::string AltelReader::readPack(const std::chrono::milliseconds &timeout_idel){ //timeout_read_interval
  return readPack(m_fd, timeout_idel);
}
