#include "MultiscatteringReference.h"
#include <iostream>
#include <iomanip>
glm::dvec2 Bounded(double mu,double r,unsigned n){
 const double a=r*r,a2=a*a,s=sqrt(1-mu*mu),sv=sqrt(mu*mu+a2*s*s),pi=acos(-1.);glm::dvec2 out(0);
 auto nodes=IBLReferenceNodes(n);
 for(unsigned j=0;j<4*n;++j){double cp=cos(2*pi*(j+.5)/(4*n)),c=s*cp;
 double top=c>=0?(c+hypot(c,mu))/mu:mu/(hypot(c,mu)-c);
 double cap=top*top/(a2+top*top);
 for(auto node:nodes){double u=cap*node.x*(2-node.x),jac=cap*2*(1-node.x),t=a*sqrt(u/(1-u));
 double hz=1/sqrt(1+t*t),vh=(mu+c*t)*hz,nl=(mu*(1-t*t)+2*c*t)/(1+t*t);
 if(nl<=0)continue;double sl=sqrt(nl*nl+a2*(1-nl*nl));
 double w=2*nl*(mu+c*t)/(nl*sv+mu*sl),fc=pow(1-vh,5);
 out+=glm::dvec2(1-fc,fc)*(w*node.y*jac/(4*n));
 }}return out;
}
int main(){std::cout<<std::setprecision(15);for(double mu:{.00001,.0001,.001,.01})for(unsigned n:{128,256,512,1024,2048}){auto e=Bounded(mu,.045,n);std::cout<<mu<<','<<n<<','<<e.x<<','<<e.y<<','<<1-e.x-e.y<<std::endl;}}
