#include "MultiscatteringReference.h"
#include <iostream>
#include <iomanip>
int main(){std::cout<<std::setprecision(15);for(double r:{.045,.046,.25,1.})for(unsigned n:{32,64,128}){glm::dvec2 e(0);for(auto node:IBLReferenceNodes(n))e+=MultiscatteringAlbedo(node.x,r)*(2*node.x*node.y);std::cout<<r<<','<<n<<','<<e.x<<','<<e.y<<','<<1-e.x-e.y<<std::endl;}}
