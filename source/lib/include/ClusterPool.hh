#ifndef _ALTEL_CLUSTERPOOL_HH
#define _ALTEL_CLUSTERPOOL_HH

#include "mysystem.hh"

#include <vector>

namespace altel{

  struct PixelHit{
    union {
      uint64_t index;
      uint16_t loc[4];
    } data{0};

    PixelHit(uint64_t h)
      :data{ .index = h }{};
    PixelHit(uint16_t x, uint16_t y, uint16_t z)
      :data{ .loc = {x, y, z, 0} }{};
  
    inline bool operator==(const PixelHit &rh){
      return data.index == rh.data.index;
    }

    inline bool operator<(const PixelHit &rh){
      return data.index < rh.data.index;
    }
  
    inline const uint64_t& index() const  {return data.index;}
    inline const uint16_t& x() const  {return data.loc[0];}
    inline const uint16_t& y() const  {return data.loc[1];}
    inline const uint16_t& z() const  {return data.loc[2];}
    inline uint64_t& index(){return data.index;}
    inline uint16_t& x(){return data.loc[0];}
    inline uint16_t& y(){return data.loc[1];}
    inline uint16_t& z(){return data.loc[2];}

  };

  struct ClusterHit{
    double   centerX{0};
    double   centerY{0};
    double   resolutionX{0};
    double   resolutionY{0};
    uint16_t pixelSize{0};
    uint16_t surfIndex{0};
  
    std::vector<PixelHit> pixelHits;
  
    ClusterHit(std::vector<PixelHit> &&hits)
      :pixelHits(std::move(hits))
    {
    };

    ClusterHit(const std::vector<PixelHit> &hits)
      :pixelHits(hits)
    {
    };
  
    inline const double& x() const{return centerX;}
    inline const double& y() const{return centerY;}
    inline const uint16_t& z() const{return surfIndex;}
    inline const uint16_t& size() const{return pixelSize;}
    inline const double& resX() const{return resolutionX;}
    inline const double& resY() const{return resolutionY;}
  
    void buildClusterCenter(){
      if(pixelSize)
        return;//already build;
      pixelSize = pixelHits.size();
      if(!pixelSize)
        return;//nothing to build;
      for(auto &ph : pixelHits){
        centerX+= ph.x();
        centerY+= ph.y();
        surfIndex = ph.z();
      }
      centerX *= 0.02924;
      centerY *= 0.02688;
      centerX /= pixelSize;
      centerY /= pixelSize;
    }
  };

  class ClusterPool{
  public:
    inline void addHit(uint16_t x, uint16_t y, uint16_t z){
      m_hits.emplace_back(x, y, z);
    }
    void buildClusters();  
    std::vector<PixelHit> m_hits;
    std::vector<ClusterHit> m_clusters;  
  };
  
}
#endif
