#include "MultiscatteringReference.h"
#include <fstream>
#include <iostream>
#include <iomanip>
struct Image {unsigned Side;std::vector<std::vector<float>> Mips;};
int main(int argc,char** argv){
 if(argc!=2)return 1;Image image;std::ifstream in(argv[1],std::ios::binary);in.seekg(16);in.read(reinterpret_cast<char*>(&image.Side),4);in.seekg(72);image.Mips={std::vector<float>(size_t(image.Side)*image.Side*2)};in.read(reinterpret_cast<char*>(image.Mips[0].data()),std::streamsize(image.Mips[0].size()*4));
 std::cout<<std::setprecision(12)<<"rough,NoV,analyticA,analyticB,sampledA,sampledB,directPerfect,directWhiteDielectric,meanA,meanB,meanPhysicalA,meanPhysicalB\n";
 double worst=0;
 for(double r:{0.,.045,.046,.047,.05,.1,.125,.25,.5,.75,.999,1.})for(double mu:{.00001,.0001,.0005,.001,.002,.005,.01,.05,.1,.25,.5,.9,1.}){
  auto physical=MultiscatteringAlbedo(mu,std::max(.045,r)),ab=ReferenceSampleRuntimeBRDF(image,mu,r),av=ReferenceAverageRuntimeBRDF(image,r),truth=MultiscatteringAverage(std::max(.045,r));
  double perfect=1+physical.x+physical.y-ab.x-ab.y,white=1+.04*(physical.x-ab.x)+physical.y-ab.y;
  worst=std::max({worst,std::abs(perfect-1),std::abs(white-1)});
  std::cout<<r<<','<<mu<<','<<physical.x<<','<<physical.y<<','<<ab.x<<','<<ab.y<<','<<perfect<<','<<white<<','<<av.x<<','<<av.y<<','<<truth.x<<','<<truth.y<<'\n';
 }
 std::cerr<<"Maximum independent energy error "<<worst<<'\n';return worst>.003?2:0;
}
