#!/usr/bin/env python3
"""Independent upstream Heitz Smith random walks versus Hazel's FP64 KC model.

Downloads only into the evidence directory; no upstream code is installed or
redistributed in Hazel. Precision promotion and Schlick throughput are explicit.
"""
import argparse
import hashlib
import io
from pathlib import Path
import re
import subprocess
import urllib.request
import zipfile

ROOT = Path(__file__).resolve().parents[2]
URL = 'https://drive.google.com/uc?export=download&id=0BzvWIdpUpRx_bTdXTEFfaWlQRUE&resourcekey=0-clibl9DJOShOiHqwVIe3vg'
SHA = '38fca8b662398676b13210611440c1ec47d937628c19e545e2945a7b330c9d8d'
HARNESS = r'''
#include <cfloat>
#include <iomanip>
#include <fstream>
#include "MicrosurfaceScattering.cpp"
#include "MultiscatteringReference.h"
// Upstream geometry/height/VNDF random walk; only track RGB Schlick attenuation.
struct WeightedConductor : MicrosurfaceConductor {
    dvec3 f0;
    mutable dvec3 throughput{1};
    WeightedConductor(double alpha,dvec3 color):MicrosurfaceConductor(true,false,alpha,alpha),f0(color){}
    dvec3 samplePhaseFunction(const dvec3& wi) const override {
        auto wo=MicrosurfaceConductor::samplePhaseFunction(wi);
        auto h=normalize(wi+wo);
        throughput*=f0+(1.0-f0)*std::pow(1-std::clamp(dot(wi,h),0.,1.),5);
        return wo;
    }
};
int main(int argc,char** argv) {
    if(argc!=2)return 1;
    std::ofstream convergence(argv[1]);
    convergence<<std::setprecision(12)<<"rough,NoV,delta128to256,delta256to512,finalOrder,finalDelta\n";
    for(double r:{.045,.25,.5,1.})for(double mu:{.01,.05,.5,1.}) {
        auto a=MultiscatteringIntegral(mu,r,128),b=MultiscatteringIntegral(mu,r,256),c=MultiscatteringIntegral(mu,r,512);
        unsigned order=512;double delta=glm::length(b-c);
        const double initial=delta;
        while(delta>1e-7 && order<4096) {
            order*=2;auto refined=MultiscatteringIntegral(mu,r,order);delta=glm::length(refined-c);c=refined;
        }
        convergence<<r<<','<<mu<<','<<glm::length(a-b)<<','<<initial<<','<<order<<','<<delta<<'\n';
        if(delta>1e-7)return 4;
    }
    convergence.flush();
    std::ofstream averages(std::string(argv[1])+".averages.csv");
    averages<<std::setprecision(12)<<"rough,meanDelta32to64,meanDelta64to128\n";
    for(double r:{.045,.25,.5,1.}) {
        dvec2 coarse(0),fine(0),finest(0);
        for(auto node:IBLReferenceNodes(32))coarse+=MultiscatteringAlbedo(node.x,r)*(2*node.x*node.y);
        for(auto node:IBLReferenceNodes(64))fine+=MultiscatteringAlbedo(node.x,r)*(2*node.x*node.y);
        for(auto node:IBLReferenceNodes(128))finest+=MultiscatteringAlbedo(node.x,r)*(2*node.x*node.y);
        const double delta=glm::length(fine-coarse),tail=glm::length(finest-fine);averages<<r<<','<<delta<<','<<tail<<'\n';
        if(delta>1e-4 || tail>1e-7)return 5;
    }
    averages.flush();
    gen.seed(20261125);
    std::cout<<std::setprecision(12)<<"rough,NoV,F0r,F0g,F0b,channel,walk,stderr,KC,singleWalk,singleFP64\n";
    for(double r:{.25,.5,1.})for(double mu:{.05,.5,1.})
      for(auto f0:{dvec3(1),dvec3(.04),dvec3(.955,.638,.538),dvec3(.95,.64,.15),dvec3(.91,.92,.92)}) {
        WeightedConductor surface(r*r,f0);dvec3 sum(0),square(0),single(0),singleSquare(0);
        constexpr int count=262144;
        for(int i=0;i<count;++i) {
            surface.throughput=dvec3(1);int order=0;
            auto exit=surface.sample(dvec3(std::sqrt(1-mu*mu),0,mu),order);
            if(!std::isfinite(exit.z)||exit.z<=0)return 2;
            auto value=surface.throughput;sum+=value;square+=value*value;
            if(order==1){single+=value;singleSquare+=value*value;}
        }
        auto ab=MultiscatteringAlbedo(mu,r);
        auto kc=ReferenceMultiscatteringEnergy(f0,ab,MultiscatteringAverage(r));
        for(int c=0;c<3;++c) {
            double mean=sum[c]/count,se=std::sqrt(std::max(0.,square[c]/count-mean*mean)/count);
            double first=single[c]/count,expected=f0[c]*ab.x+ab.y;
            // Same Smith first-bounce geometry is a prerequisite to comparing models.
            double firstSE=std::sqrt(std::max(0.,singleSquare[c]/count-first*first)/count);
            if(std::abs(first-expected)>.0005+6*firstSE)return 3;
            std::cout<<r<<','<<mu<<','<<f0.r<<','<<f0.g<<','<<f0.b<<','<<c<<','<<mean<<','<<se<<','<<kc.Reflected[c]<<','<<first<<','<<expected<<'\n';
        }
    }
}
'''

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output',type=Path,required=True)
    args=parser.parse_args(); out=args.output.resolve();out.mkdir(parents=True,exist_ok=True)
    archive=out/'upstream.zip'
    if not archive.exists():archive.write_bytes(urllib.request.urlopen(URL).read())
    data=archive.read_bytes()
    if hashlib.sha256(data).hexdigest()!=SHA:raise RuntimeError('Upstream archive identity changed')
    with zipfile.ZipFile(io.BytesIO(data)) as z:
        for name in ('MicrosurfaceScattering.h','MicrosurfaceScattering.cpp'):
            raw=z.read('multipleScatteringSmith_tutorial/'+name)
            source=raw.decode('utf-16') if raw.startswith(b'\xff\xfe') else raw.decode('utf-8-sig')
            source=source.replace('\r\n','\n')
            for before,after in (('float','double'),('vec2','dvec2'),('vec3','dvec3'),('FLT_MAX','DBL_MAX'),('erf','SmithErf')):
                source=re.sub(r'\b'+before+r'\b',after,source)
            source=re.sub(r'\b(sqrt|pow|sin|cos|tan|acos|asin|atan|atan2|exp|log|fabs)f\b',r'\1',source)
            source=re.sub(r'(\d(?:\.\d*)?(?:e[+-]?\d+)?)f\b',r'\1',source,flags=re.I)
            source=source.replace('static std::random_device rd;','').replace('std::mt19937 gen(rd())','std::mt19937 gen(20261125)')
            # Host portability repairs: delete through the two upstream abstract
            # base pointers must be defined; keep the scattering algorithm intact.
            if name.endswith('.h'):
                for base in ('MicrosurfaceHeight','MicrosurfaceSlope'):
                    source=source.replace('class '+base+'\n{\npublic:',
                        'class '+base+'\n{\npublic:\n    virtual ~'+base+'() = default;')
            else:
                source=source.replace('#define M_PI\t','#undef M_PI\n#define M_PI\t').replace('#define M_PI_2\t','#undef M_PI_2\n#define M_PI_2\t')

            (out/name).write_text(source)
    (out/'reference.cpp').write_text(HARNESS)
    command=['clang++','-std=c++17','-O2','-include','cfloat','-I'+str(ROOT/'Hazel/vendor/glm'),'-I'+str(ROOT/'tests/migration'),str(out/'reference.cpp'),'-o',str(out/'reference')]
    with (out/'build.log').open('w') as log:subprocess.run(command,stdout=log,stderr=subprocess.STDOUT,check=True)
    with (out/'comparison.csv').open('w') as log:subprocess.run([out/'reference',out/'convergence.csv'],stdout=log,check=True)
    (out/'provenance.txt').write_text(f'URL {URL}\nSHA256 {SHA}\n262144 paths per case; seed 20261125; FP64 promotion; abstract-base virtual destructors and host macro repairs; upstream VNDF approximations retained.\n'+repr(command)+'\n')
    print('PASS: independent Smith walk completed; comparison.csv records approximation error, not an equality gate')
if __name__=='__main__':main()
