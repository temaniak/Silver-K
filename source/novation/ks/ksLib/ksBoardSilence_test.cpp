// Explicit retained LEGACY comparison test; not a semantic NativeControllerExecutor proof.
#include "ksBoardComparison_test.h"
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <vector>

std::vector<uint8_t> bytes(const char* path) { std::ifstream f(path,std::ios::binary); return {(std::istreambuf_iterator<char>(f)),{}}; }
void require(bool condition,const char* reason) { if(!condition)throw std::runtime_error(reason); }
int main(int argc,char** argv) {
 if(argc!=4)return 2;
 try {
    auto os=bytes(argv[1]),bank=bytes(argv[2]),flash=bytes(argv[3]);
    require(flash.size()==0x80000,"Bad test Flash");
    // One known factory INIT program, with a deterministic original-controller
    // patch at bank 1/00 to exercise Program Change and subsequent Note On.
    std::copy_n(flash.begin()+0x2c800,256,flash.begin()+0x20000);
    flash[0x20000+128]=27;
    flash[0x20000+37]=127;
    flash[0x20000+38]=flash[0x20000+39]=flash[0x20000+40]=flash[0x20000+41]=0;
    flash[0x20000+62]=flash[0x20000+65]=0;
    flash[0x20000+100]=flash[0x20000+107]=flash[0x20000+110]=0;
    {
        auto pattern=bank;pattern.front()=0x12;pattern[1]=0x34;pattern[pattern.size()-2]=0xab;pattern.back()=0xcd;
        ks::Board mapped(os,pattern,flash);
        require(!mapped.zeroBank(),"Real wave bank enabled adaptation");
        require(ks::test::comparisonCpu(mapped).read16(ks::Board::WaveBase)==0x1234 && ks::test::comparisonCpu(mapped).read16(ks::Board::WaveBase+ks::Board::WaveSize-2)==0xabcd,"Wave bank word reads lost byte order or boundaries");
    }
    std::copy_n(flash.begin()+0x20000,256,flash.begin()+0x39000);
    flash[0x39000+128]=31;
    flash[0x1ff02]=1;flash[0x1ff03]=0;flash[0x1ff04]=0;
    ks::Board b(os,bank,flash);
    require(b.read(0x820000+37)==0 && b.read(0x839000+37)==0,"Sampled Flash must be muted before reset");
    require(b.saveUserFlash()[0x20000-0x1fc00+37]==127,"Prepared Flash lost desired level");
    require(b.boot(),"Original reset did not boot");
    require(b.zeroBank(),"Expected placeholder adapter");
    require(b.midiReceived<512,"Boot rewrote the stored bank over MIDI");
    require(b.state().parts[0][37]==127,"Sampled startup program lost desired level");
    auto midi=[&](std::initializer_list<uint8_t> m){std::vector<uint8_t> v(m);require(b.enqueueMidi(v.data(),v.size()),"MIDI queue overflow");};
    auto settle=[&](unsigned n=8192){for(unsigned i=0;i<n;++i)b.renderSample();};
    int voicePeak=0;auto peak=[&](){int p=0;voicePeak=0;for(unsigned i=0;i<8192;++i){auto a=b.renderSample();voicePeak=std::max(voicePeak,std::abs(a[19]));p=std::max(p,std::max(std::abs(a[0]),std::abs(a[1])));}return p;};
    midi({0xb0,7,127,73,0,74,0,75,0,76,0,91,0,92,0,93,0,108,0,111,0});settle();
    for(uint8_t osc=0;osc<3;++osc) {
        for(uint8_t wave=8;wave<32;++wave) {
            midi({0xb0,uint8_t(72+osc),127,98,uint8_t(50+osc),6,wave,
                  98,uint8_t(63+osc),6,127}); // Deliberately try to unmute by LFO/AD modulation.
            settle();midi({0x90,69,100});
            const auto measured=peak();if(measured)std::cerr<<"osc="<<unsigned(osc)<<" wave="<<unsigned(wave)<<" peak="<<measured<<" voice="<<voicePeak<<" levels="<<unsigned(b.read(0x168fc+37))<<","<<unsigned(b.read(0x168fc+38))<<","<<unsigned(b.read(0x168fc+39))<<" ring="<<unsigned(b.read(0x168fc+41))<<" fm="<<unsigned(b.read(0x168fc+56))<<" noise="<<unsigned(b.read(0x168fc+40))<<" mods="<<unsigned(b.read(0x168fc+141))<<","<<unsigned(b.read(0x168fc+142))<<","<<unsigned(b.read(0x168fc+143))<<"\n";require(measured==0,"ROM wave or level modulation produced audio");
            midi({0x80,69,0});settle();
        }
        std::cout<<"oscillator="<<unsigned(osc+1)<<" all 24 ROM waves: exact silence\n"<<std::flush;
    }
    midi({0xb0,32,1,0xc0,0,0x90,69,100});settle();
    require(peak()==0,"Program Change unmuted ROM preset");
    require(b.read(0x168fc+128)==27 && b.read(0x168fc+37)==0,"Original firmware did not apply preset/mute commands");
    std::array<uint8_t,256> saved{};for(unsigned i=0;i<256;++i)saved[i]=b.read(0x168fc+i);b.restoreDesiredLevels(saved.data());
    require(saved[37]==127,"Desired preset level was not preserved");
    midi({0x80,69,0,0xb0,32,5,0xc0,0,0x90,36,100});settle();
    require(peak()==0,"Drum-bank Program Change unmuted ROM preset");
    require(b.read(0x168fc+128)==31 && b.read(0x168fc+37)==0,"Drum-bank preset/mute commands did not apply");
    midi({0x80,36,0,0xb0,32,1,0xc0,0,0x90,69,100});settle();
    std::array<uint8_t,270> current{0xf0,0,0x20,0x29,1,0x42,0x7f,0,0,0x11,2,0,0};
    std::copy_n(flash.data()+0x20000,256,current.data()+13);current[13+128]=31;current.back()=0xf7;
    require(b.enqueueMidi(current.data(),current.size()),"Current preset queue overflow");
    require(peak()==0,"Current preset emitted DC while a ROM note was held");
    midi({0xb0,32,1,0xc0,0});require(peak()==0,"Program Change emitted DC while a ROM note was held");
    const auto savedFlash=b.saveUserFlash();
    require(savedFlash[0x39000-0x1fc00+37]==127 && savedFlash[0x39000-0x1fc00+128]==31,"Saved Flash lost desired drum parameters");
    midi({0x80,36,0,0xb0,32,1,0xc0,0});settle();
    for(uint8_t osc=0;osc<3;++osc) {
        midi({0xb0,uint8_t(72+osc),127,98,uint8_t(50+osc),6,27});settle();
        midi({0xb0,98,uint8_t(50+osc),6,2,98,uint8_t(63+osc),6,64});settle();
        midi({0x90,69,100});require(peak()>1000,"Returning to generated wave lost its level");
        midi({0x80,69,0,0xb0,uint8_t(72+osc),0});settle();
    }
    std::cout<<"PASS: 72 oscillator/wave cases, modulation, generated-wave restoration, original Program Change, saved desired level\n";
 } catch(const std::exception& e) { std::cerr<<e.what()<<'\n';return 1; }
}
