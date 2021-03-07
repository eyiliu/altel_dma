#include"ClusterPool.hh"

#include <algorithm>

using namespace altel;

void ClusterPool::buildClusters(){
  auto hit_col_remain= m_hits;

  while(!hit_col_remain.empty()){
    std::vector<PixelHit> hit_col_this_cluster;
    std::vector<PixelHit> hit_col_this_cluster_edge;
      
    // get first edge seed hit
    // from un-identifed hit to edge hit
    hit_col_this_cluster_edge.push_back(hit_col_remain[0]);
    hit_col_remain.erase(hit_col_remain.begin());
      
    while(!hit_col_this_cluster_edge.empty()){
      auto ph_e = hit_col_this_cluster_edge[0];
      uint64_t e = ph_e.index();
      uint64_t c = 0x00000001;
      uint64_t r = 0x00010000;
        
      //  8 sorround hits search, 
      std::vector<PixelHit> sorround_col
        {e-c+r, e+r, e+c+r,
         e-c  ,      e+c,
         e-c-r, e-r, e+c-r
        };
        
      for(auto& sr: sorround_col){
        // only search on un-identifed hits
        auto sr_found_it = std::find(hit_col_remain.begin(), hit_col_remain.end(), sr);
        if(sr_found_it != hit_col_remain.end()){
          // move the found sorround hit
          // from un-identifed hit to an edge hit
          hit_col_this_cluster_edge.push_back(sr);
          hit_col_remain.erase(sr_found_it);
        }
      }
        
      // after sorround search
      // move from edge hit to cluster hit
      hit_col_this_cluster.push_back(e);
      hit_col_this_cluster_edge.erase(hit_col_this_cluster_edge.begin());  
    } 
    m_clusters.emplace_back(std::move(hit_col_this_cluster));
  }

  for(auto &c : m_clusters){
    c.buildClusterCenter();
  }
}
