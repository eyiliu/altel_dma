#ifndef _ALTEL_DATAFRAME_HH_
#define _ALTEL_DATAFRAME_HH_

#include <string>
#include <vector>
#include <memory>

#include "mysystem.hh"
#include "myrapidjson.h"

#include "ClusterPool.hh"

using namespace altel; // to be removed

namespace altel{
  class DataFrame;
  using DataFrameSP = std::shared_ptr<DataFrame>;

  class DataFrame {
  public:
    DataFrame(const std::string& raw);
    DataFrame(std::string&& raw);
    DataFrame(const rapidjson::Value &js);
    DataFrame(const rapidjson::GenericValue<rapidjson::UTF8<>, rapidjson::CrtAllocator> &js);
    DataFrame(){};

  
    inline void SetTrigger(uint64_t v){m_trigger = v;};
    inline uint64_t GetTrigger(){return m_trigger;};
    inline uint64_t GetCounter(){return m_counter;}
    inline uint64_t GetExtension(){return m_extension;}
  
    void Print(std::ostream& os, size_t ws = 0) const;

    template <typename Allocator>
    rapidjson::GenericValue<rapidjson::UTF8<>, Allocator> JSON(Allocator &a) const;

    template <typename Allocator>
    void fromJSON(const rapidjson::GenericValue<rapidjson::UTF8<>, Allocator> &js);
  
    void fromRaw(const std::string &raw, uint32_t level = 4);
  
    static const uint16_t s_version{4};
    std::string m_raw;
    uint16_t m_level_decode{0};
    uint64_t m_counter{0};
    uint64_t m_extension{0};
    uint64_t m_trigger{0};
    std::vector<ClusterHit> m_clusters;
  };

  template <typename Allocator>
  rapidjson::GenericValue<rapidjson::UTF8<>, Allocator> DataFrame::JSON(Allocator &a) const{
    rapidjson::GenericValue<rapidjson::UTF8<>, Allocator> js;
    js.SetObject();
    js.AddMember("det", "alpide", a);
    js.AddMember("ver",  rapidjson::GenericValue<rapidjson::UTF8<>, Allocator>(s_version), a);
    js.AddMember("tri",  rapidjson::GenericValue<rapidjson::UTF8<>, Allocator>(m_trigger), a);
    js.AddMember("cnt",  rapidjson::GenericValue<rapidjson::UTF8<>, Allocator>(m_counter), a);
    js.AddMember("ext",  rapidjson::GenericValue<rapidjson::UTF8<>, Allocator>(m_extension), a);

    rapidjson::GenericValue<rapidjson::UTF8<>, Allocator> js_cluster_hits;
    js_cluster_hits.SetArray();
    for(auto &ch : m_clusters){
      rapidjson::GenericValue<rapidjson::UTF8<>, Allocator> js_cluster_hit;
      js_cluster_hit.SetObject();

      rapidjson::GenericValue<rapidjson::UTF8<>, Allocator> js_cluster_hit_pos;
      js_cluster_hit_pos.SetArray();
      // must enable  RAPIDJSON_HAS_CXX11_RVALUE_REFS 1
      js_cluster_hit_pos.PushBack(rapidjson::GenericValue<rapidjson::UTF8<>, Allocator>(ch.x()), a);
      js_cluster_hit_pos.PushBack(rapidjson::GenericValue<rapidjson::UTF8<>, Allocator>(ch.y()), a);
      js_cluster_hit_pos.PushBack(rapidjson::GenericValue<rapidjson::UTF8<>, Allocator>(ch.z()), a);
      js_cluster_hit.AddMember("pos", std::move(js_cluster_hit_pos), a);

      rapidjson::GenericValue<rapidjson::UTF8<>, Allocator> js_cluster_hit_res;
      js_cluster_hit_res.SetArray();
      js_cluster_hit_res.PushBack(rapidjson::GenericValue<rapidjson::UTF8<>, Allocator>(ch.resX()), a);
      js_cluster_hit_res.PushBack(rapidjson::GenericValue<rapidjson::UTF8<>, Allocator>(ch.resY()), a);
      js_cluster_hit.AddMember("res", std::move(js_cluster_hit_res), a);
      
      rapidjson::GenericValue<rapidjson::UTF8<>, Allocator> js_pixel_hits;
      js_pixel_hits.SetArray();
      for(auto &ph : ch.pixelHits){
        rapidjson::GenericValue<rapidjson::UTF8<>, Allocator> js_pixel_hit_pos;
        js_pixel_hit_pos.SetArray();
        js_pixel_hit_pos.PushBack(rapidjson::GenericValue<rapidjson::UTF8<>, Allocator>(ph.x()), a);
        js_pixel_hit_pos.PushBack(rapidjson::GenericValue<rapidjson::UTF8<>, Allocator>(ph.y()), a);
        js_pixel_hit_pos.PushBack(rapidjson::GenericValue<rapidjson::UTF8<>, Allocator>(ph.z()), a);
        js_pixel_hits.PushBack(std::move(js_pixel_hit_pos), a);
      }
      js_cluster_hit.AddMember("pix", std::move(js_pixel_hits), a);
      
      js_cluster_hits.PushBack(std::move(js_cluster_hit), a);
    }
    js.AddMember("hit", std::move(js_cluster_hits) , a);
    
    //https://rapidjson.org/classrapidjson_1_1_generic_object.html
    //https://rapidjson.org/md_doc_tutorial.html#CreateModifyValues
    return js;
  };
  
  template <typename Allocator>
  void DataFrame::fromJSON(const rapidjson::GenericValue<rapidjson::UTF8<>, Allocator> &js){
    if(js["ver"].GetUint64()!=s_version){
      std::fprintf(stderr, "mismathed data writer/reader versions");
      throw;
    }

    m_trigger   = js["tri"].GetUint64();
    m_counter   = js["cnt"].GetUint64();
    m_extension = js["ext"].GetUint64();
    
    const auto &js_chs = js["hit"].GetArray();
    for(const auto &js_ch : js_chs){
      std::vector<PixelHit> pixelhits;
      const auto &js_phs = js_ch["pix"].GetArray();
      for(const auto &js_ph : js_phs){
        const auto &js_pos = js_ph.GetArray();
        pixelhits.emplace_back(js_pos[0].GetUint(),
                               js_pos[1].GetUint(),
                               js_pos[2].GetUint());
      }
      ClusterHit clusterhit(std::move(pixelhits));
      clusterhit.buildClusterCenter();
      m_clusters.push_back(std::move(clusterhit));
    }
    m_level_decode = 5;
  }

  class DataPack{
  public:
    uint64_t m_trigger{0};
    std::vector<DataFrameSP> m_frames;
  };

  using DataPackSP = std::shared_ptr<DataPack>;
}

#endif

