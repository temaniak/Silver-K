#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>

namespace ks {
// Worker-owned causal ADC converter. Prepare coefficients before audio starts.
class AudioInput {
public:
    static constexpr size_t Taps=64, Phases=256;
    static constexpr unsigned Delay=31; // host samples, external input only
    AudioInput(double hostRate,double deviceRate) {
        const double cutoff=0.94*std::min(1.0,deviceRate/hostRate);
        constexpr double pi=3.14159265358979323846;
        for(size_t p=0;p<=Phases;++p){
            double sum=0;
            for(size_t n=0;n<Taps;++n){
                const double x=double(n)-Delay-double(p)/Phases;
                const double a=pi*cutoff*x;
                const double window=0.42-0.5*std::cos(2*pi*n/(Taps-1))+0.08*std::cos(4*pi*n/(Taps-1));
                const double value=cutoff*(std::abs(a)<1e-12?1.0:std::sin(a)/a)*window;
                coefficients[p][n]=float(value);sum+=value;
            }
            for(auto& value:coefficients[p])value=float(value/sum);
        }
    }
    static float finite(float value) noexcept {return std::isfinite(value)?value:0.0f;}
    static float mono(float left,float right) noexcept {return finite(left)*0.5f+finite(right)*0.5f;}
    static int32_t word(float value) noexcept {
        return int32_t(std::clamp(finite(value),-1.0f,0.99999988f)*8388608.0f);
    }
    void push(float sample) noexcept {
        if(history[cursor]!=0)--nonzero;
        history[cursor]=finite(sample);
        if(history[cursor]!=0)++nonzero;
        cursor=(cursor+1)%Taps;
    }
    int32_t sample(double lag) const noexcept {
        if(!nonzero)return 0;
        const auto row=size_t(std::clamp(lag,0.0,1.0)*Phases);
        double value=0;
        for(size_t n=0;n<Taps;++n)value+=double(history[(cursor+Taps-1-n)%Taps])*coefficients[row][n];
        return word(float(value));
    }
private:
    std::array<std::array<float,Taps>,Phases+1> coefficients{};
    std::array<float,Taps> history{};
    size_t cursor=0;
    size_t nonzero=0;
};
}
