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
  AltelReader();

  DataFrameSP Read(const std::chrono::milliseconds &timeout);

  std::vector<DataFrameSP> Read(size_t size_max_pkg,
                                const std::chrono::milliseconds &timeout_idle,
                                const std::chrono::milliseconds &timeout_total);


  std::string readPack(const std::chrono::milliseconds &timeout_idel);
    
  bool Open();
  void Close();
  static std::string LoadFileToString(const std::string& path);

  const std::string& DeviceUrl();
private:
  int m_fd{0};
  std::string m_file_path;
  JsonDocument m_jsdoc_conf;
};

#endif
