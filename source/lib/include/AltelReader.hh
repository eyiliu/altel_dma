#ifndef _ALPIDE_READER_WS_
#define _ALPIDE_READER_WS_

#include <string>
#include <vector>
#include <chrono>

#include "mysystem.hh"
#include "DataFrame.hh"

class AltelReader{
public:
  ~AltelReader();
  AltelReader(const rapidjson::GenericValue<rapidjson::UTF8<>, rapidjson::CrtAllocator> &js);
  AltelReader(const std::string &json_string);

  DataFrameSP Read(const std::chrono::milliseconds &timeout);

  DataFrameSP ReadRaw(size_t len, const std::chrono::milliseconds &timeout_idel);

  std::vector<DataFrameSP> Read(size_t size_max_pkg,
                                const std::chrono::milliseconds &timeout_idel,
                                const std::chrono::milliseconds &timeout_total);
  bool Open();
  void Close();
  static std::string LoadFileToString(const std::string& path);

  const std::string& DeviceUrl();
private:
  int m_fd{0};
  std::string m_tcp_ip;
  uint16_t m_tcp_port{24};
  std::string m_file_path;
  bool m_file_terminate_eof{false};
  bool m_flag_file{false};

  rapidjson::CrtAllocator m_jsa;
  rapidjson::GenericValue<rapidjson::UTF8<>, rapidjson::CrtAllocator> m_js_conf;
};

#endif
