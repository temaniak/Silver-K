#pragma once
#include "ksBoard.h"
#include <algorithm>
#include <cmath>

namespace ks::test {
// Test-only external input pins. All bus timing, ADC IRQ scheduling, controller
// scan work, DSP transport and Flash handling remain owned by the actual Board.
class PanelIoFixture final : public ControllerBus {
public:
    explicit PanelIoFixture(Board& board):board_(board){
        rows_.fill(255);adc_.fill(0x8000);
        baseCounter_=uint16_t((unsigned(board_.read(0xfffff6))<<8)|board_.read(0xfffff7));
        counter_=baseCounter_;
    }
    void configureScreens(unsigned edit,unsigned next,unsigned previous,bool encoder=false){screenEncoderMode_=encoder;screenKeys_={edit,next,next,next,next,next,previous,next,edit};screenMode_=true;}
    void configureSequence(const std::array<unsigned,9>& keys,bool encoder=false){screenKeys_=keys;screenEncoderMode_=encoder;screenMode_=true;}
    void configureKeyboard(){keyboardMode_=true;keyboardWords_.fill(0);}
    void apply(uint64_t frame,double rate,unsigned storeKey,bool multi){
        const auto at=[&](double seconds){return uint64_t(std::llround(seconds*rate));};
        if(keyboardMode_){std::array<uint16_t,8> next{};for(unsigned i=0;i<3;++i){const double begin=.4+i*.8;unsigned contacts=0;if(frame>=at(begin)&&frame<at(begin+.42))contacts=frame<at(begin+.02)||frame>=at(begin+.4)?2u:3u;const unsigned page=i==0?0:i==1?3:7,key=i==0?0:i==1?3:7;next[page]|=uint16_t(contacts<<(2*key));}for(unsigned i=0;i<8;++i)if(next[i]!=keyboardWords_[i])++keyboardTransitions;keyboardWords_=next;}
        rows_.fill(255);bool pressed=false;
        for(unsigned press=0;press<(screenMode_?unsigned(screenKeys_.size()):(multi?3u:4u));++press)
            if(frame>=at(.2+press*(screenMode_?.42:.35))&&frame<at(.26+press*(screenMode_?.42:.35))){pressed=true;const auto key=screenMode_?screenKeys_[press]:storeKey;rows_[key/8]&=uint8_t(~(1u<<(key%8)));}
        if(pressed!=pressed_){++buttonTransitions;pressed_=pressed;}
        const bool pedal=(frame>=at(.12)&&frame<at(.4))||(frame>=at(1.6)&&frame<at(1.9));
        port_=pedal?0xbf:0xff;if(pedal!=pedal_){++pedalTransitions;pedal_=pedal;}
        int detents=0;
        if((!screenMode_||screenEncoderMode_)&&frame>=at(2.2)){
            const unsigned step=unsigned((frame-at(2.2))/std::max<uint64_t>(1,at(.02)));
            detents=step<24?int(step):step<56?48-int(step):-8;
        }
        const auto counter=uint16_t(unsigned(baseCounter_)+unsigned(detents*4));
        if(counter!=counter_)++encoderTransitions;counter_=counter;
        for(unsigned channel=0;channel<4;++channel){
            const auto value=!screenMode_&&frame>=at(1.8)&&frame<at(3.8)?uint16_t(0x4000+((unsigned((frame-at(1.8))/500)*971+channel*7193)&0x7fff)):uint16_t(0x8000);
            if(value!=adc_[channel])++adcTransitions;adc_[channel]=value;
        }
    }
    uint8_t read(uint32_t address)override{
        if(keyboardMode_&&(address==0xffff55||address==0xffff54||address==0xffff59||address==0xffff50)){
            unsigned page=0;while(page<7&&!(keyboardMux_&(1u<<page)))++page;const auto value=keyboardWords_[page];++keyboardReads;
            if(address==0xffff55)return uint8_t(value>>8)&0xfc;if(address==0xffff54)return uint8_t((value>>8)&3)<<2;if(address==0xffff59)return uint8_t(value)&0xe0;++pedalReads;return uint8_t((port_&0xe0)|(value&31));
        }
        if(address==0xffff51){
            const auto mux=board_.read(0xffff62)&0xf7;
            if(mux>=0x20&&mux<0x26){++matrixReads;return rows_[mux-0x20];}
        }
        if(address==0xffff50){++pedalReads;return port_;}
        if(address==0xfffff6||address==0xfffff7){++encoderReads;return uint8_t(address&1?counter_:counter_>>8);}
        if(address>=0xffff90&&address<0xffff98){++adcReads;const auto value=adc_[(address-0xffff90)/2];return uint8_t(address&1?value:value>>8);}
        return board_.read(address);
    }
    void write(uint32_t address,uint8_t value)override{if(keyboardMode_&&address==0xffff6d)keyboardMux_=value;board_.write(address,value);}
    uint64_t keyboardReads=0,keyboardTransitions=0;
    uint64_t matrixReads=0,pedalReads=0,encoderReads=0,adcReads=0;
    uint64_t buttonTransitions=0,pedalTransitions=0,encoderTransitions=0,adcTransitions=0;
private:
    bool keyboardMode_=false;uint8_t keyboardMux_=1;std::array<uint16_t,8> keyboardWords_{};
    bool screenEncoderMode_=false;bool screenMode_=false;std::array<unsigned,9> screenKeys_{};
    Board& board_;std::array<uint8_t,6> rows_{};std::array<uint16_t,4> adc_{};
    uint16_t baseCounter_=0,counter_=0;uint8_t port_=255;bool pressed_=false,pedal_=false;
};
}
