// Analog '83 performance benchmark: cost of each DSP stage and of the whole
// kit while playing, idle, and in the denormal worst case. Build in Release.
#include "Analog83DemoPattern.h"
#include "Analog83Kit.h"
#include "Analog83Record.h"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <memory>
#include <vector>
using namespace a83;
using clk = std::chrono::steady_clock;
static volatile float sink;
template <typename F> double nsPerSample(int samples, F&& f){ auto t=clk::now(); f(); double ns=std::chrono::duration<double,std::nano>(clk::now()-t).count(); return ns/samples; }
int main(){
  const int N=48000*20; const double sr=48000;
  printf("PER-STAGE COST (ns per sample, one channel unless noted)\n");
  { Kick k; k.prepare(sr); k.setDecay(2.0f); std::vector<float> b(256);
    printf("  kick voice (sounding)    %6.1f\n", nsPerSample(N,[&]{ for(int p=0;p<N;p+=256){ if(!k.isActive()) k.trigger(1); std::fill(b.begin(),b.end(),0.f); k.process(b.data(),256);} sink=b[0];})); }
  { HiHat h; h.prepare(sr); h.setOpenDecay(2.0f); std::vector<float> b(256);
    printf("  hat voice (sounding)     %6.1f\n", nsPerSample(N,[&]{ for(int p=0;p<N;p+=256){ if(!h.isActive()) h.triggerOpen(1); std::fill(b.begin(),b.end(),0.f); h.process(b.data(),256);} sink=b[0];})); }
  { BusCompressor c; c.prepare(sr); c.setAmount(0.15f); float acc=0; Noise n;
    printf("  bus compressor gain      %6.1f\n", nsPerSample(N,[&]{ for(int i=0;i<N;i++) acc+=c.gainFor(std::abs(n.next())); sink=acc;})); }
  { Saturator s; s.setAmount(0.18f); float acc=0; Noise n;
    printf("  saturator                %6.1f\n", nsPerSample(N,[&]{ for(int i=0;i<N;i++) acc+=s.process(n.next()); sink=acc;})); }
  { Room r; r.prepare(sr); float l,rr,acc=0; Noise n;
    printf("  room (stereo)            %6.1f\n", nsPerSample(N,[&]{ for(int i=0;i<N;i++){ r.process(n.next()*0.1f,l,rr); acc+=l+rr;} sink=acc;})); }
  { RecordChain rc; rc.prepare(sr); rc.setAmount(0.6f); float acc=0; Noise n;
    printf("  record chain (stereo)    %6.1f\n", nsPerSample(N,[&]{ for(int i=0;i<N;i++){ float l=n.next()*0.3f,r=l; rc.process(l,r); acc+=l+r;} sink=acc;})); }
  { float acc=0; Noise n;
    printf("  soft clip                %6.1f\n", nsPerSample(N,[&]{ for(int i=0;i<N;i++) acc+=softClip(n.next()*1.2f); sink=acc;})); }

  printf("WHOLE KIT, stereo out (ns per sample; 20833 ns = 100%% of one core at 48 kHz)\n");
  auto kitRun=[&](bool play, float room, float record, bool oneHitThenSilence){
    auto kp=std::make_unique<Kit>(); Kit& k=*kp; k.prepare(sr); k.setParam(Param::Room,room); k.setParam(Param::Record,record);
    DemoPattern demo; demo.prepare(sr);
    std::vector<float> L(256),R(256);
    if(oneHitThenSilence){ k.trigger(VoiceId::OpenHat,1); for(int p=0;p<48000*3;p+=256){ std::fill(L.begin(),L.end(),0.f); std::fill(R.begin(),R.end(),0.f); k.process(L.data(),R.data(),256);} }
    return nsPerSample(N,[&]{ for(int p=0;p<N;p+=256){ if(play) demo.schedule(k,256); std::fill(L.begin(),L.end(),0.f); std::fill(R.begin(),R.end(),0.f); k.process(L.data(),R.data(),256);} sink=L[0];});
  };
  double a=kitRun(true,0,60,false), b=kitRun(false,0,60,false), c=kitRun(true,100,60,false), d=kitRun(false,100,0,true);
  printf("  demo beat playing, defaults         %6.1f  (%.2f%% of a core)\n",a,a/208.33);
  printf("  idle, nothing playing, defaults     %6.1f  (%.2f%% of a core)\n",b,b/208.33);
  printf("  beat playing, ROOM 100%%             %6.1f  (%.2f%% of a core)\n",c,c/208.33);
  printf("  silence after a hit, ROOM 100%%, RECORD 0 (denormal test) %6.1f\n",d);
}
