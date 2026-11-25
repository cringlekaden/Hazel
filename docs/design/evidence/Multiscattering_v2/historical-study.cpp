#include "MultiscatteringReference.h"
#include <fstream>
#include <iostream>
#include <iomanip>
struct Image {unsigned Side=128;std::vector<std::vector<float>> Mips{std::vector<float>(32768)};};
int main(int argc,char** argv){
 if(argc!=2)return 1;Image image;std::ifstream in(argv[1],std::ios::binary);in.seekg(72);in.read(reinterpret_cast<char*>(image.Mips[0].data()),32768*4);
 std::cout<<std::setprecision(12)<<"rough,NoV,metal,channel,directEnergy,IBLenergy,analyticA,analyticB,sampledA,sampledB,meanA,meanB,clippedMeanDeficit\n";
 for(double r:{.045,.046,.1,.25,.5,.75,.99609375,1.})for(double mu:{.0001,.00390625,.01,.01171875,.05,.25,.5,.99609375,1.})for(double metal:{0.,1.}){
  glm::dvec3 f0(metal?1:.04);auto av=ReferenceAverageBRDF(image,r),ab=ReferenceSampleBRDF(image,mu,r);
  auto e=ReferenceMultiscatteringEnergy(f0,ab,av);double missing=0,meanQ=0,left=0;
  for(unsigned interval=0;interval<=128;++interval){double right=interval==128?1:(interval+.5)/128;
   for(auto node:IBLReferenceNodes(8)){double m=left+(right-left)*node.x;auto li=ReferenceSampleBRDF(image,m,r);auto energy=ReferenceMultiscatteringEnergy(f0,li,av);double w=2*m*(right-left)*node.y;
    missing+=std::max(0.,1-li.x-li.y)*w;meanQ+=energy.Residual*w;
   }left=right;
  }
  auto physical=MultiscatteringAlbedo(mu,r);auto result=f0*physical.x+physical.y;
  if(av.x+av.y<1)result+=e.C*(std::max(0.,1-ab.x-ab.y)*missing/(1-av.x-av.y));
  if(e.AverageResidual>0)result+=glm::dvec3((1-metal)*e.Residual*meanQ/e.AverageResidual);
  for(int c=0;c<3;++c)std::cout<<r<<','<<mu<<','<<metal<<','<<c<<','<<result[c]<<','<<e.Reflected[c]+(1-metal)*e.Residual<<','<<physical.x<<','<<physical.y<<','<<ab.x<<','<<ab.y<<','<<av.x<<','<<av.y<<','<<missing<<'\n';
 }
}
