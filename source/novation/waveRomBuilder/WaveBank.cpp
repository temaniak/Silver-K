#include "WaveBank.h"
#include <algorithm>
#include <cmath>
#include <complex>
#include <limits>
#include <numeric>
#include <stdexcept>

namespace waveRom
{
namespace {
constexpr double pi=3.14159265358979323846;
void require(bool ok,const char* error){if(!ok)throw std::runtime_error(error);}
float at(const std::vector<float>& x,double p){if(x.empty())return 0;p=std::clamp(p,0.,double(x.size()-1));auto a=size_t(p);return float(x[a]+(x[std::min(a+1,x.size()-1)]-x[a])*(p-a));}
void fft(std::vector<std::complex<double>>& a,bool inverse)
{
    const auto n=a.size();
    for(size_t i=1,j=0;i<n;++i){auto bit=n>>1;for(;j&bit;bit>>=1)j^=bit;j^=bit;if(i<j)std::swap(a[i],a[j]);}
    for(size_t len=2;len<=n;len*=2){auto wlen=std::polar(1.,(inverse?2:-2)*pi/len);for(size_t i=0;i<n;i+=len){std::complex<double>w=1;for(size_t j=0;j<len/2;++j){auto u=a[i+j],v=a[i+j+len/2]*w;a[i+j]=u+v;a[i+j+len/2]=u-v;w*=wlen;}}}
    if(inverse)for(auto& v:a)v/=double(n);
}
}
const std::array<Slot,24>& slots()
{
    // Structural layout of the supported KS wave loader, not ROM contents.
    static const std::array<Slot,24> table=[] {
        const char* names[]{"Organ","Harpsichord","Electric Piano","Slap Bass","Rhodes Piano","Rhodes Tine","Whirly EP","Clavinet","Analogue Bass","Kick 1","Kick 2","Kick 3","Snare 1","Snare 2","Snare 3","Tom Tom (Front)","Rimshot","Stick","Closed Hi-Hat","Open Hi-Hat","Tambourine","Conga","Cow Bell","Cymbal"};
        const int counts[]{1024,1024,1024,1024,1024,1024,1024,1024,1024,2048,2048,2048,8192,8192,8192,2048,2048,1024,1024,4096,4096,2048,2048,16384};
        std::array<Slot,24> result{};size_t offset=0;
        for(int i=0;i<24;++i){const auto bits=(i==19||i==23)?8:16;result[size_t(i)]={i+8,counts[i],bits,offset,names[i]};offset+=size_t(counts[i]*bits/8);}
        require(offset==bankSize,"Invalid slot layout");return result;
    }();return table;
}
bool Selection::valid(size_t n) const { return std::isfinite(begin)&&std::isfinite(end)&&begin>=0&&end<=double(n)&&end-begin>=8; }
Selection findCycle(const std::vector<float>& x,double rate)
{
    require(rate>=8000&&rate<=384000&&x.size()>=64,"Not enough audio to find a cycle");
    double peak=0;for(auto v:x){require(std::isfinite(v),"Audio contains non-finite samples");peak=std::max(peak,std::abs(double(v)));}
    require(peak>1e-7,"The recording is silent");
    size_t onset=0;while(onset<x.size()&&std::abs(x[onset])<peak*.03)++onset;
    auto start=std::min(onset+size_t(rate*.025),x.size()/2);
    double stride=std::max(1.,rate/12000.);
    size_t n=std::min(size_t(6144),size_t((x.size()-start)/stride));
    require(n>=64,"Recording is too short for cycle detection");
    size_t fftSize=1;while(fftSize<n*2)fftSize*=2;
    std::vector<std::complex<double>> a(fftSize);
    std::vector<double> energy(n+1);double mean=0;
    for(size_t i=0;i<n;++i)mean+=at(x,start+i*stride);mean/=double(n);
    for(size_t i=0;i<n;++i){double v=at(x,start+i*stride)-mean;a[i]=v;energy[i+1]=energy[i]+v*v;}
    fft(a,false);for(auto& v:a)v=std::norm(v);fft(a,true);
    auto minLag=std::max(size_t(3),size_t(rate/stride/2000));
    auto maxLag=std::min(n/3,size_t(rate/stride/20));
    require(maxLag>minLag+2&&energy.back()>1e-12,"No periodic region found");
    std::vector<double> score(maxLag+2);
    double best=-1;size_t bestLag=minLag;
    for(size_t k=minLag-1;k<=maxLag+1;++k){auto norm=std::sqrt(energy[n-k]*(energy[n]-energy[k]));score[k]=norm>1e-12?a[k].real()/norm:0;if(k>=minLag&&k<=maxLag&&score[k]>best){best=score[k];bestLag=k;}}
    // First strong local maximum avoids choosing a multiple of the period.
    for(size_t k=minLag;k<maxLag;++k)if(score[k]>=score[k-1]&&score[k]>score[k+1]&&score[k]>=std::max(.78,best*.97)){bestLag=k;break;}
    require(best>.45,"No clear repeating cycle: set the boundaries manually");
    const auto divisor=score[bestLag-1]-2*score[bestLag]+score[bestLag+1];
    const auto fraction=std::abs(divisor)>1e-12?std::clamp(.5*(score[bestLag-1]-score[bestLag+1])/divisor,-.5,.5):0;
    double period=(bestLag+fraction)*stride;
    // Refine in original sample coordinates, including fractional boundaries.
    auto error=[&](double lag){double e=0;const auto limit=std::min(size_t(rate*.08),x.size()-start-size_t(std::ceil(lag))-2);for(size_t j=0;j<limit;j+=3){double d=at(x,start+j)-at(x,start+j+lag);e+=d*d;}return e/std::max(size_t(1),limit);};
    double lo=std::max(8.,period-stride),hi=period+stride;
    for(int i=0;i<22;++i){double p=lo+(hi-lo)/3,q=hi-(hi-lo)/3;if(error(p)<error(q))hi=q;else lo=p;}
    period=(lo+hi)*.5;
    double boundary=start,bestSeam=std::numeric_limits<double>::infinity();
    auto end=std::min(x.size()-size_t(std::ceil(period))-2,start+size_t(rate*.12));
    for(size_t i=start+1;i<end;++i){if(x[i-1]<=0&&x[i]>0){double point=i-1+(-double(x[i-1]))/(x[i]-x[i-1]);double value=at(x,point+period);double slope=(at(x,point+period+1)-at(x,point+period-1))-(at(x,point+1)-at(x,point-1));double cost=value*value+slope*slope*.25;if(cost<bestSeam){bestSeam=cost;boundary=point;}}}
    Selection result{boundary,boundary+period,std::clamp(best,0.,1.)};require(result.valid(x.size()),"Cannot fit a complete cycle");return result;
}
Selection findTransient(const std::vector<float>& x,double rate)
{
    require(rate>=8000&&rate<=384000&&x.size()>=8,"Not enough audio to find an attack");
    const auto window=std::max(size_t(1),size_t(rate*.001));
    std::vector<double> rms((x.size()+window-1)/window);double peak=0;
    for(size_t k=0;k<rms.size();++k){double sum=0;auto end=std::min(x.size(),(k+1)*window);for(size_t i=k*window;i<end;++i){require(std::isfinite(x[i]),"Audio contains non-finite samples");sum+=double(x[i])*x[i];}rms[k]=std::sqrt(sum/(end-k*window));peak=std::max(peak,rms[k]);}
    require(peak>1e-7,"The recording is silent");
    size_t first=0;while(first<rms.size()&&rms[first]<peak*.04)++first;
    size_t tail=first,quiet=0;const auto minHold=size_t(rate*.025/window),gap=size_t(rate*.015/window);
    for(size_t k=first;k<rms.size();++k){if(rms[k]>peak*.003){tail=k+1;quiet=0;}else ++quiet;if(k>first+minHold&&quiet>gap)break;}
    double begin=std::max(0.,double(first*window)-rate*.001),end=std::min(double(x.size()),double(tail*window)+rate*.003);
    if(end-begin<8)end=std::min(double(x.size()),begin+8);return {begin,end,1};
}
std::vector<float> resample(const std::vector<float>& x,size_t count,bool periodic)
{
    require(!x.empty()&&count>0&&count<=16000000,"Invalid resampling length");
    std::vector<float> result(count);const double ratio=double(x.size())/count,cutoff=std::min(1.,1/ratio)*.94;
    const int radius=int(std::ceil(20/cutoff));
    for(size_t i=0;i<count;++i){double p=i*ratio,sum=0,weight=0;auto center=int64_t(std::floor(p));for(int j=-radius;j<=radius;++j){auto k=center+j;double d=p-double(k);if(std::abs(d)>radius)continue;double u=pi*d*cutoff;double sinc=std::abs(u)<1e-12?cutoff:std::sin(u)/(pi*d);double w=sinc*(.5+.5*std::cos(pi*d/radius));weight+=w;if(periodic){auto index=(k%int64_t(x.size())+int64_t(x.size()))%int64_t(x.size());sum+=x[size_t(index)]*w;}else if(k>=0&&k<int64_t(x.size()))sum+=x[size_t(k)]*w;}result[i]=weight!=0?float(sum/weight):0;}
    return result;
}
std::vector<float> prepare(const std::vector<float>& x,double rate,Selection selection,const Slot& slot,Processing processing)
{
    require(selection.valid(x.size())&&std::isfinite(rate)&&rate>0,"Invalid selection");
    require(std::isfinite(processing.peakDb)&&processing.peakDb>=-60&&processing.peakDb<=-.2&&std::isfinite(processing.fadeInMs)&&std::isfinite(processing.fadeOutMs)&&processing.fadeInMs>=0&&processing.fadeOutMs>=0,"Invalid processing settings");
    const auto length=selection.end-selection.begin;auto count=size_t(std::ceil(length));std::vector<float> cut(count);
    for(size_t i=0;i<count;++i){cut[i]=at(x,selection.begin+i*length/count);require(std::isfinite(cut[i]),"Audio contains non-finite samples");}
    if(slot.tonal()){double dc=std::accumulate(cut.begin(),cut.end(),0.)/cut.size();for(auto& v:cut)v-=float(dc);}
    else {const double in=std::min(double(count-1),processing.fadeInMs*.001*rate),out=std::min(double(count-1),processing.fadeOutMs*.001*rate);for(size_t i=0;i<count;++i){double gain=1;if(in>0)gain*=std::min(1.,i/in);if(out>0)gain*=std::min(1.,(count-1-i)/out);cut[i]*=float(gain);}}
    auto result=resample(cut,size_t(slot.samples),slot.tonal());
    double peak=0;for(auto v:result)peak=std::max(peak,std::abs(double(v)));
    if(processing.normalize&&peak>1e-12){double gain=std::pow(10.,processing.peakDb/20)/peak;for(auto& v:result)v*=float(gain);}
    else require(peak<=.98,"Result exceeds ROM headroom: enable normalization or lower the WAV level");
    // Audition the actual quantized ROM values, not a higher precision proxy.
    for(auto& v:result){double scale=slot.bits==8?128:32768;double pcm=std::round(-v*scale);pcm=std::clamp(pcm,slot.bits==8?-128.:-32768.,slot.bits==8?127.:32767.);v=float(-pcm/scale);}
    return result;
}
double seamError(const std::vector<float>& x){if(x.size()<4)return 0;return std::abs(double(x.front())-x.back())+std::abs(double(x[1]-x[0])-(x.back()-x[x.size()-2]));}
Bank::Bank():raw(bankSize,0){for(const auto& s:slots())if(s.bits==8)std::fill(raw.begin()+int64_t(s.offset),raw.begin()+int64_t(s.offset+s.bytes()),128);}
void Bank::load(const std::vector<uint8_t>& data){require(data.size()==bankSize,"Wave ROM must contain exactly 129024 bytes");raw=data;zeroPlaceholder=std::all_of(raw.begin(),raw.end(),[](auto v){return v==0;});}
std::vector<float> Bank::decode(size_t index) const
{
    const auto& s=slots().at(index);std::vector<float> x(size_t(s.samples));if(zeroPlaceholder)return x;
    for(size_t i=0;i<x.size();++i){if(s.bits==8)x[i]=float(128-int(raw[s.offset+i]))/128;else {int v=int(raw[s.offset+2*i])*256+raw[s.offset+2*i+1];if(v>=32768)v-=65536;x[i]=float(-v)/32768;}}return x;
}
void Bank::put(size_t index,const std::vector<float>& x)
{
    const auto& s=slots().at(index);require(x.size()==size_t(s.samples),"Wrong slot length");for(auto v:x)require(std::isfinite(v),"Non-finite ROM sample");
    if(zeroPlaceholder)*this=Bank();
    for(size_t i=0;i<x.size();++i){if(s.bits==8){auto v=std::clamp(int(std::lround(-double(x[i])*128)),-128,127)+128;raw[s.offset+i]=uint8_t(v);}else {auto v=uint16_t(std::clamp(int(std::lround(-double(x[i])*32768)),-32768,32767));raw[s.offset+2*i]=uint8_t(v>>8);raw[s.offset+2*i+1]=uint8_t(v);}}
}
double noteHz(int note){return 440*std::pow(2.,(std::clamp(note,0,127)-69)/12.);}
std::string noteName(int note){static const char* names[]{"C","C#","D","D#","E","F","F#","G","G#","A","A#","B"};note=std::clamp(note,0,127);return std::string(names[note%12])+std::to_string(note/12-1);}
std::unique_ptr<PreviewBuffer> tonalPreview(const std::vector<float>& x,int note,double rate)
{
    require(!x.empty()&&rate>=8000,"No audio to audition");auto result=std::make_unique<PreviewBuffer>();size_t n=1;while(n<x.size())n*=2;
    result->pcm=resample(x,n,true);std::vector<std::complex<double>> bins(n);for(size_t i=0;i<n;++i)bins[i]=result->pcm[i];fft(bins,false);
    auto harmonics=size_t(std::floor(rate*.47/noteHz(note)));for(size_t i=1;i<n;++i)if(std::min(i,n-i)>harmonics)bins[i]=0;fft(bins,true);for(size_t i=0;i<n;++i)result->pcm[i]=float(bins[i].real());
    result->step=noteHz(note)*n/rate;result->loop=true;return result;
}
std::unique_ptr<PreviewBuffer> samplePreview(const std::vector<float>& x,double sourceRate,int note,double rate,bool loop)
{
    require(!x.empty()&&std::isfinite(sourceRate)&&sourceRate>0&&rate>=8000,"No audio to audition");
    const auto length=double(x.size())*rate/sourceRate/std::pow(2.,(note-60)/12.);require(length>=1&&length<=16000000,"Preview is too long at this note");
    auto result=std::make_unique<PreviewBuffer>();result->pcm=resample(x,size_t(std::llround(length)),false);result->loop=loop;return result;
}
bool PreviewVoice::publish(std::unique_ptr<PreviewBuffer> p){collect();if(!p||p->pcm.empty())return false;if(!incoming.push(p.get()))return false;p.release();return true;}
void PreviewVoice::collect(){while(auto* p=retired.pop())delete p;}
PreviewVoice::~PreviewVoice(){collect();while(auto* p=incoming.pop())delete p;delete current;}
void PreviewVoice::render(float* const* outputs,int channels,int count,float gain) noexcept
{
    const bool stopped=stopRequested.exchange(false,std::memory_order_acq_rel);
    // Bound command work even if the GUI publishes concurrently. The retired
    // queue exceeds two full incoming queues; publish drains it before enqueue.
    for(int update=0;update<7;++update){auto* p=incoming.pop();if(!p)break;if(current){const bool ok=retired.push(current);(void)ok;}carry=last;current=p;position=0;ramp=0;release=0;active.store(true,std::memory_order_relaxed);}
    if(stopped)release=64;
    for(int i=0;i<count;++i){float value=0;if(active.load(std::memory_order_relaxed)&&current){const auto n=current->pcm.size();if(position>=n){if(current->loop)position=std::fmod(position,double(n));else active.store(false,std::memory_order_relaxed);}if(active.load(std::memory_order_relaxed)){auto a=size_t(position),b=a+1;if(b>=n)b=current->loop?0:n-1;float pcm=float(current->pcm[a]+(current->pcm[b]-current->pcm[a])*(position-a));if(ramp<64)++ramp;float attack=float(ramp)/64;value=pcm*attack+carry*(1-attack);if(release){value*=float(release)/64;if(--release==0)active.store(false,std::memory_order_relaxed);}position+=current->step;}}last=value;for(int c=0;c<channels;++c)outputs[c][i]=value*gain;}
}
}
