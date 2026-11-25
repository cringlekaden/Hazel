#include "MultiscatteringReference.h"
#include <fstream>
#include <iostream>
#include <iomanip>
struct Image {unsigned Side;std::vector<std::vector<float>> Mips;};
int main(int argc,char** argv){
 if(argc!=2)return 1;Image image;std::ifstream in(argv[1],std::ios::binary);in.seekg(16);in.read(reinterpret_cast<char*>(&image.Side),4);in.seekg(72);image.Mips={std::vector<float>(size_t(image.Side)*image.Side*2)};in.read(reinterpret_cast<char*>(image.Mips[0].data()),std::streamsize(image.Mips[0].size()*4));
 std::cout<<std::setprecision(15)<<"x,y,rough,NoV,analyticA,analyticB,storedA,storedB,error\n";double worst=0;
 for(unsigned y:{0u,1u,2u,17u,64u,128u,254u,255u})for(unsigned x:{32u,64u,117u,128u,192u,220u,254u,255u}){
  double r=.045+.955*y/255.,mu=std::pow(x/255.,4);auto physical=MultiscatteringAlbedo(mu,r);size_t p=(size_t(y)*256+x)*2;
  glm::dvec2 stored(image.Mips[0][p],image.Mips[0][p+1]);double error=std::max(std::abs(physical.x-stored.x),std::abs(physical.y-stored.y));worst=std::max(worst,error);
  std::cout<<x<<','<<y<<','<<r<<','<<mu<<','<<physical.x<<','<<physical.y<<','<<stored.x<<','<<stored.y<<','<<error<<std::endl;
 }
 std::cerr<<"Maximum collocated independent A/B error "<<worst<<std::endl;return worst>5e-7?2:0;
}
