#ifndef _TELESCOPE_HH_
#define _TELESCOPE_HH_

#include <mutex>
#include <future>
#include <cstdio>

#include "FirmwarePortal.hh"
#include "AltelReader.hh"
#include "myrapidjson.h"
#include "Layer.hh"

namespace altel{
  class Telescope{
  public:
    std::vector<std::unique_ptr<Layer>> m_vec_layer;
    std::future<uint64_t> m_fut_async_rd;
    std::future<uint64_t> m_fut_async_watch;
    bool m_is_async_reading{false};
    bool m_is_async_watching{false};
    bool m_is_running{false};

    std::vector<DataFrameSP> m_ev_last;
    std::vector<DataFrameSP> m_ev_last_empty;
    std::atomic<uint64_t> m_mon_ev_read{0};
    std::atomic<uint64_t> m_mon_ev_write{0};
    std::vector<DataFrameSP> ReadEvent_Lastcopy();

    std::atomic<uint64_t> m_st_n_ev{0};

    ~Telescope();
    Telescope(const std::string& file_context);
    std::vector<DataFrameSP> ReadEvent();

    void Init();
    void Start();
    void Stop();
    void Start_no_tel_reading();
    uint64_t AsyncRead();
    uint64_t AsyncWatchDog();

    bool m_flag_next_event_add_conf{true};

    rapidjson::CrtAllocator m_jsa;
    rapidjson::GenericValue<rapidjson::UTF8<char>, rapidjson::CrtAllocator> m_js_testbeam;
    rapidjson::GenericValue<rapidjson::UTF8<char>, rapidjson::CrtAllocator> m_js_telescope;

    rapidjson::GenericValue<rapidjson::UTF8<char>, rapidjson::CrtAllocator>  m_js_status;
    std::atomic<uint64_t> m_count_st_js_write{0};
    std::atomic<uint64_t> m_count_st_js_read{0};
  };
}
#endif
