#include "MultiscatteringReference.h"
#include <fstream>
#include <iostream>
#include <iomanip>
struct Image {unsigned Side;std::vector<std::vector<float>> Mips;};
int main(int argc,char**argv){if(argc!=2)return 1;Image i;std::ifstream in(argv[1],std::ios::binary);in.seekg(16);in.read(reinterpret_cast<char*>(&i.Side),4);in.seekg(72);i.Mips={std::vector<float>(size_t(i.Side)*i.Side*2)};in.read(reinterpret_cast<char*>(i.Mips[0].data()),std::streamsize(i.Mips[0].size()*4));std::cout<<std::setprecision(15);for(double r:{.045,.046}){auto v=glm::dvec3(sqrt(1-.01*.01),0,.01),l=glm::normalize(glm::dvec3(.3,.4,1));auto a=OracleMultiscatteringBRDF({1,1,1},1,r,{0,0,1},v,l)*2.;auto b=OracleRuntimeMultiscatteringBRDF(i,{1,1,1},1,r,{0,0,1},v,l)*2.;auto av=MultiscatteringAverage(r),bv=ReferenceAverageRuntimeBRDF(i,r);std::cout<<r<<','<<a.x<<','<<b.x<<','<<av.x<<','<<av.y<<','<<bv.x<<','<<bv.y<<std::endl;}}
