#include "MultiscatteringReference.h"
#include <fstream>
#include <iostream>
#include <iomanip>
struct Image {unsigned Side;std::vector<std::vector<float>> Mips;};
int main(int argc,char** argv){
 if(argc!=2)return 1;Image image;std::ifstream in(argv[1],std::ios::binary);in.seekg(16);in.read(reinterpret_cast<char*>(&image.Side),4);in.seekg(72);image.Mips={std::vector<float>(size_t(image.Side)*image.Side*2)};in.read(reinterpret_cast<char*>(image.Mips[0].data()),std::streamsize(image.Mips[0].size()*4));

 const double mu=.01,r=.045,alpha=r*r,a2=alpha*alpha,pi=std::acos(-1.);
 const glm::dvec3 v(std::sqrt(1-mu*mu),0,mu),tangent(0,1,0);
 auto stretched=glm::normalize(glm::dvec3(alpha*v.x,0,mu)),bitangent=glm::cross(stretched,tangent);
 double blend=(1+stretched.z)*.5,sv=std::sqrt(mu*mu+a2*(1-mu*mu));
 auto av=ReferenceAverageRuntimeBRDF(image,r),abv=ReferenceSampleRuntimeBRDF(image,mu,r);
 for(unsigned samples:{8192u,16384u,32768u}) {
  double sum=0;
  for(unsigned tech=0;tech<2;++tech)for(unsigned i=0;i<samples;++i){
   double u=(i+.5)/samples,phi=2*pi*std::fmod((i+.5)*.6180339887498948482,1.);glm::dvec3 l;
   if(tech==0){double x=std::sqrt(u)*std::cos(phi),y=(1-blend)*std::sqrt(1-x*x)+blend*std::sqrt(u)*std::sin(phi);auto disk=tangent*x+bitangent*y+stretched*std::sqrt(std::max(0.,1-x*x-y*y));auto h=glm::normalize(glm::dvec3(alpha*disk.x,alpha*disk.y,std::max(0.,disk.z)));l=glm::reflect(-v,h);}
   else l={std::sqrt(u)*std::cos(phi),std::sqrt(u)*std::sin(phi),std::sqrt(1-u)};
   if(l.z<=0)continue;auto h=glm::normalize(v+l);double den=1-h.z*h.z+a2*h.z*h.z,pdf=a2/(pi*den*den)/(2*(mu+sv))+l.z/pi;
   auto abl=ReferenceSampleRuntimeBRDF(image,l.z,r);
   double direct=OracleBRDF({1,1,1},1,r,{0,0,1},v,l).x;
   direct+=(1-abv.x-abv.y)*(1-abl.x-abl.y)*l.z/(pi*(1-av.x-av.y));sum+=direct/(pdf*samples);
  }
  std::cout<<samples<<' '<<std::setprecision(12)<<sum<<'\n';
 }
}
