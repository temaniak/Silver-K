#include "ksAudioInput.h"
#include <iostream>
#include <limits>
#include <stdexcept>

static void check(bool value,const char* text){if(!value)throw std::runtime_error(text);}
static double tone(double rate,double hz){
    ks::AudioInput converter(rate,45340.401785714);
    const double ratio=45340.401785714/rate;
    double phase=0,energy=0;unsigned count=0;
    for(unsigned n=0;n<unsigned(rate);++n){
        converter.push(float(0.25*std::sin(2*3.14159265358979323846*hz*n/rate)));
        phase+=ratio;
        while(phase>=1){phase-=1;const double sample=converter.sample(phase/ratio)/8388608.;
            check(std::isfinite(sample),"Non-finite ADC sample");
            if(n>256){energy+=sample*sample;++count;}}
    }
    return std::sqrt(energy/count);
}
int main(){try{
    check(ks::AudioInput::word(std::numeric_limits<float>::quiet_NaN())==0,"NaN");
    check(ks::AudioInput::word(std::numeric_limits<float>::infinity())==0,"Infinity");
    check(ks::AudioInput::word(-2)==-8388608 && ks::AudioInput::word(2)==8388607,"24-bit saturation");
    check(ks::AudioInput::mono(0.5f,-0.5f)==0,"Stereo cancellation");
    for(double rate:{44100.,48000.,96000.,192000.}){
        ks::AudioInput converter(rate,45340.401785714);
        for(unsigned n=0;n<2000;++n){converter.push(0);check(converter.sample(0.5)==0,"Silence");}
        for(unsigned n=0;n<128;++n)converter.push(0.25f);
        for(unsigned p=0;p<=256;++p)check(std::abs(converter.sample(p/256.)-2097152)<=1,"DC gain");
        const auto rms=tone(rate,1000);check(rms>0.175 && rms<0.179,"Audible-band gain");
        if(rate>=96000){const auto alias=tone(rate,40000);check(alias<0.001,"ADC anti-alias rejection");}
    }
    std::cout<<"PASS: ADC silence, DC, finite/clipping, stereo cancellation, 44.1/48/96/192 kHz passband and anti-alias rejection\n";
    return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
