#pragma once

#include <mutex>
#include <future>

#include <cstdio>

#include "FirmwarePortal.hh"
#include "AltelReader.hh"

#include "myrapidjson.h"

namespace altel{
  class Layer{
  public:
    std::unique_ptr<FirmwarePortal> m_fw;
    std::unique_ptr<AltelReader> m_rd;
    std::future<uint64_t> m_fut_async_rd;
    std::future<uint64_t> m_fut_async_watch;
    std::vector<DataFrameSP> m_vec_ring_ev;
    DataFrameSP m_ring_end;

    uint64_t m_size_ring{200000};
    std::atomic<uint64_t> m_count_ring_write;
    std::atomic<uint64_t> m_hot_p_read;
    uint64_t m_count_ring_read;
    bool m_is_async_reading{false};
    bool m_is_async_watching{false};

    uint64_t m_extension{0};

    //status variable:
    std::atomic<uint64_t> m_st_n_tg_ev_now{0};
    std::atomic<uint64_t> m_st_n_ev_input_now{0};
    std::atomic<uint64_t> m_st_n_ev_output_now{0};
    std::atomic<uint64_t> m_st_n_ev_bad_now{0};
    std::atomic<uint64_t> m_st_n_ev_overflow_now{0};
    std::atomic<uint64_t> m_st_n_tg_ev_begin{0};

    uint64_t m_st_n_tg_ev_old{0};
    uint64_t m_st_n_ev_input_old{0};
    //uint64_t m_st_n_ev_output_old{0};
    uint64_t m_st_n_ev_bad_old{0};
    uint64_t m_st_n_ev_overflow_old{0};
    std::chrono::system_clock::time_point m_tp_old;
    std::chrono::system_clock::time_point m_tp_run_begin;

    std::string m_st_string;
    std::mutex m_mtx_st;
  public:
    ~Layer();

    void fw_start();
    uint64_t fw_async_restart();
    void fw_stop();
    void fw_init();
    void fw_conf();

    void rd_start();
    void rd_stop();

    uint64_t AsyncPushBack();
    DataFrameSP GetNextCachedEvent();
    DataFrameSP& Front();
    void PopFront();
    uint64_t Size();
    void ClearBuffer();

    std::string GetStatusString();
    uint64_t AsyncWatchDog();

    std::string m_name;
    rapidjson::CrtAllocator m_jsa;
    rapidjson::GenericValue<rapidjson::UTF8<char>, rapidjson::CrtAllocator> m_js_conf;
  };
}
