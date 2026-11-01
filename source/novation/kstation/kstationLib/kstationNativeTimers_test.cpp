#include "kstationNativeTimers.h"
#include "h8s/h8sdevices.hpp"

#include <array>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <memory>
#include <new>
#include <stdexcept>
#if defined(_MSC_VER)
#include <malloc.h>
#endif

namespace
{
bool countAllocations = false;
std::uint64_t allocations = 0;
void* allocate(std::size_t size)
{
    if(countAllocations) ++allocations;
    if(auto* p = std::malloc(size ? size : 1)) return p;
    throw std::bad_alloc();
}
}
void* operator new(std::size_t n) { return allocate(n); }
void* operator new[](std::size_t n) { return allocate(n); }
void operator delete(void* p) noexcept { std::free(p); }
void operator delete[](void* p) noexcept { std::free(p); }
void operator delete(void* p, std::size_t) noexcept { std::free(p); }
void operator delete[](void* p, std::size_t) noexcept { std::free(p); }
void* operator new(std::size_t n, std::align_val_t alignment)
{
    if(countAllocations) ++allocations;
#if defined(_MSC_VER)
    if(auto* p = _aligned_malloc(n ? n : 1, static_cast<std::size_t>(alignment))) return p;
#else
    void* p = nullptr;
    if(posix_memalign(&p, static_cast<std::size_t>(alignment), n ? n : 1) == 0) return p;
#endif
    throw std::bad_alloc();
}
void* operator new[](std::size_t n, std::align_val_t a) { return ::operator new(n, a); }
void operator delete(void* p, std::align_val_t) noexcept
{
#if defined(_MSC_VER)
    _aligned_free(p);
#else
    std::free(p);
#endif
}
void operator delete[](void* p, std::align_val_t a) noexcept { ::operator delete(p, a); }
void operator delete(void* p, std::size_t, std::align_val_t a) noexcept { ::operator delete(p, a); }
void operator delete[](void* p, std::size_t, std::align_val_t a) noexcept { ::operator delete(p, a); }

namespace
{
constexpr std::uint32_t Base = 0xffff60;
constexpr std::array<unsigned, 5> Starts{{4, 14, 24, 34, 50}};
std::uint64_t seed = 0x6b73746174696f6eull;
std::uint32_t randomValue()
{
    seed ^= seed << 13; seed ^= seed >> 7; seed ^= seed << 17;
    return static_cast<std::uint32_t>(seed);
}
void check(bool value, const char* why)
{
    if(!value) throw std::runtime_error(why);
}

// The independent reference is the unchanged shared peripheral with exactly
// the old K-only MDF adapter. No native arithmetic is used by this oracle.
class OriginalKTimers final : public Timers
{
public:
    void tick()
    {
        const auto start = Timers::read(Base);
        const bool phase = (Timers::read(Base + 2) & 0x40) != 0;
        if(phase) Timers::write(Base, start & ~4);
        Timers::tick();
        if(phase) Timers::write(Base, start);
    }
};
struct Access { std::uint64_t clock; std::uint32_t address; std::uint8_t value; bool write; };
template<class Timer> class Audit final : public H8SDevice
{
public:
    explicit Audit(Timer& timer) : timer_(timer) {}
    std::array<Access, 128> accesses{};
    unsigned count = 0;
    std::uint8_t read(std::uint32_t address) override
    {
        const auto value = timer_.read(address);
        check(count < accesses.size(), "Audit read capacity");
        accesses[count++] = {state->cycles, address, value, false}; return value;
    }
    void write(std::uint32_t address, std::uint8_t value) override
    {
        check(count < accesses.size(), "Audit write capacity");
        accesses[count++] = {state->cycles, address, value, true}; timer_.write(address, value);
    }
private:
    Timer& timer_;
};
class TestState final : public h8state
{
public:
    using h8state::lastread;
    using h8state::lastwrite;
};
struct Metrics {std::uint64_t cases=0,ticks=0,comparisons=0,referenceIncrements=0,mmioAccesses=0;};
class Fixture
{
public:
    Fixture(TestState& a, TestState& b, Metrics& m) : left(a), right(b), metrics(m), nativeAudit(native), referenceAudit(reference)
    {
        left.cycles=right.cycles=0;
        left.lastread=left.lastwrite=right.lastread=right.lastwrite=-1;
        left.pending_irqs=right.pending_irqs=0;
        left.pending_irqs_high=right.pending_irqs_high=0;
        left.interruptMode2=right.interruptMode2=false;
        native.setState(&left); reference.setState(&right);
        left.memmap(&nativeAudit, Base, 64); right.memmap(&referenceAudit, Base, 64);
        ++metrics.cases; same();
    }
    void write(unsigned offset, unsigned value)
    { native.write(Base+offset, static_cast<std::uint8_t>(value)); reference.write(Base+offset, static_cast<std::uint8_t>(value)); }
    void word(unsigned offset, unsigned value) {write(offset,value>>8);write(offset+1,value);}
    void channel(unsigned i,unsigned tcr,unsigned tcnt,unsigned gra,unsigned grb,unsigned tier=7,unsigned tsr=0xf8)
    {
        const auto s=Starts[i];write(s,tcr);write(s+2,tier);write(s+3,tsr);
        word(s+4,tcnt);word(s+6,gra);word(s+8,grb);
    }
    void arbitraryRegisters()
    {for(unsigned a=0;a<64;++a)write(a,randomValue());same();}
    void same()
    {
        check(left.cycles==right.cycles,"Timer changed guest clocks");
        check(left.pending_irqs==right.pending_irqs && left.pending_irqs_high==right.pending_irqs_high,"Pending IRQ mismatch");
        check(left.lastread==right.lastread && left.lastwrite==right.lastwrite,"MMIO history mismatch");
        for(unsigned a=0;a<64;++a)check(native.read(Base+a)==reference.read(Base+a),"Timer register mismatch");
        check(native.read(Base-1)==reference.read(Base-1) && native.read(Base+64)==reference.read(Base+64),"Out-of-range read mismatch");
        ++metrics.comparisons;
    }
    void tickAt(std::uint64_t now)
    {
        const auto last = lastTick_;
        unsigned active=native.read(Base);const auto mode=native.read(Base+2);
        if(mode&0x40)active&=~4u;
        for(unsigned i=0;i<5;++i)
        {
            const auto tcr=native.read(Base+Starts[i]);
            if((active&(1u<<i)) && !(mode&(1u<<i)) && !(tcr&4))
            {
                const auto inc=static_cast<std::uint32_t>((now>>(tcr&3))-(last>>(tcr&3)));
                check(inc<=0x7fffffffu,"Reference signed loop increment domain exceeded");
                metrics.referenceIncrements+=inc;
            }
        }
        left.cycles=right.cycles=now;
        native.tick(); reference.tick(); lastTick_=now; ++metrics.ticks; same();
    }
    void external(unsigned i)
    {native.tick_extclock(static_cast<int>(i));reference.tick_extclock(static_cast<int>(i));same();}
    void mmioRoundtrip(unsigned offset,unsigned value,bool longAccess)
    {
        nativeAudit.count=referenceAudit.count=0;
        if(longAccess)
        {
            left.write32(value,Base+offset);right.write32(value,Base+offset);
            check(left.read32(Base+offset)==right.read32(Base+offset),"MMIO read32 mismatch");
        }
        else
        {
            left.write16(static_cast<std::uint16_t>(value),Base+offset);right.write16(static_cast<std::uint16_t>(value),Base+offset);
            check(left.read16(Base+offset)==right.read16(Base+offset),"MMIO read16 mismatch");
        }
        check(nativeAudit.count==referenceAudit.count,"MMIO access count mismatch");
        for(unsigned i=0;i<nativeAudit.count;++i)
        {
            const auto& a=nativeAudit.accesses[i];const auto& b=referenceAudit.accesses[i];
            check(a.clock==b.clock && a.address==b.address && a.value==b.value && a.write==b.write,"MMIO byte order/time mismatch");
        }
        metrics.mmioAccesses+=nativeAudit.count; same();
    }
    TestState& left; TestState& right; Metrics& metrics;
    std::uint64_t lastTick_=0;
    kstation::NativeTimers native; OriginalKTimers reference;
    Audit<kstation::NativeTimers> nativeAudit;
    Audit<OriginalKTimers> referenceAudit;
};
}

int main(int argc,char** argv)
{
    try
    {
        static_assert(sizeof(unsigned int)==4,"Original timer increment is 32-bit");
        auto left=std::make_unique<TestState>();auto right=std::make_unique<TestState>();
        Metrics metrics;
        countAllocations=true;
        constexpr std::array<unsigned,7> values{{0,1,2,7,0x7fff,0xfffe,0xffff}};
        for(unsigned channel=0;channel<5;++channel)
        for(unsigned mode: {0u,0x20u,0x40u,0x60u})
        for(unsigned shift=0;shift<4;++shift)
        for(auto start:values)for(auto a:values)for(auto b:values)
        {
            Fixture f(*left,*right,metrics);
            f.write(0,1u<<channel);f.write(2,0x80);
            f.channel(channel,mode|shift,start,a,b);
            for(unsigned delta: {0u,1u,2u,7u,17u})f.tickAt(right->cycles+delta);
            f.external(channel);f.tickAt(right->cycles+13);
        }
        const auto boundaryCases=metrics.cases;
        for(unsigned trial=0;trial<12000;++trial)
        {
            Fixture f(*left,*right,metrics);f.arbitraryRegisters();
            left->pending_irqs=right->pending_irqs=std::uint64_t(randomValue())<<24;
            left->pending_irqs_high=right->pending_irqs_high=randomValue();
            for(unsigned tick=0;tick<8;++tick)
            {
                const auto offset=randomValue()%64;f.write(offset,randomValue());
                f.tickAt(right->cycles+randomValue()%257);
                if(tick==3)f.external(randomValue()%5);
            }
        }
        const auto randomCases=metrics.cases-boundaryCases;
        for(unsigned trial=0;trial<320;++trial)
        {
            Fixture f(*left,*right,metrics);f.arbitraryRegisters();
            const auto channel=randomValue()%5;f.write(0,1u<<channel);f.write(2,0x80);
            const auto shift=randomValue()%4;
            f.channel(channel,(randomValue()&0xfb)|shift,randomValue(),randomValue(),randomValue(),randomValue(),randomValue());
            f.tickAt((randomValue()%0x40001u)<<shift);
            // All divisors produce a delta divisible by 2^32: old inc is zero.
            f.tickAt(right->cycles+(std::uint64_t(1)<<35));
            f.tickAt(right->cycles+0x10001);
        }
        const auto largeCases=metrics.cases-boundaryCases-randomCases;
        for(unsigned trial=0;trial<512;++trial)
        {
            Fixture f(*left,*right,metrics);f.arbitraryRegisters();
            f.write(0,0);f.tickAt(~std::uint64_t(0)-4096);
            f.write(0,31);f.write(2,(trial&1)?0xc0:0x80);
            for(unsigned i=0;i<5;++i)f.channel(i,(trial+i)%4,randomValue(),randomValue(),randomValue(),7);
            f.tickAt(4096); // Truncated shifted-delta across uint64 cycle wrap.
            f.write(2,(trial&1)?0x80:0xc0);f.tickAt(8192);
            f.write(0,0);f.tickAt(100000);f.write(0,31);f.tickAt(100003);
        }
        const auto wrapCases=metrics.cases-boundaryCases-randomCases-largeCases;
        for(unsigned trial=0;trial<2048;++trial)
        {
            Fixture f(*left,*right,metrics);f.arbitraryRegisters();
            for(unsigned i=0;i<8;++i)f.mmioRoundtrip(randomValue()%61,randomValue(),(i&1)!=0);
            f.tickAt(right->cycles+91);
        }
        // Explicit old-counter0 overflow, simultaneous compares, both clear
        // selectors, MDF leave/resume, skipped-channel lastCycles and GRA wrap.
        for(unsigned mode: {0u,0x20u,0x40u,0x60u})
        {
            Fixture f(*left,*right,metrics);f.write(0,31);f.write(2,0x80);
            for(unsigned i=0;i<5;++i)f.channel(i,mode,0,0,0);
            f.tickAt(1);f.tickAt(2);f.write(2,0xc0);f.tickAt(4096);f.write(2,0x80);f.tickAt(4097);
            for(unsigned i=0;i<5;++i){f.word(Starts[i]+6,0xffff);f.external(i);}
        }
        countAllocations=false;
        check(allocations==0,"Processing allocated");
        char report[2048];
        std::snprintf(report,sizeof(report),
            "{\"schema\":\"kstation.native-timers.v1\",\"passed\":true,\"boundary_cases\":%llu,\"random_cases\":%llu,\"large_cases\":%llu,\"cycle_wrap_cases\":%llu,\"total_cases\":%llu,\"ticks\":%llu,\"comparisons\":%llu,\"reference_counter_increments\":%llu,\"mmio_byte_accesses\":%llu,\"processing_new_allocations\":%llu}\n",
            static_cast<unsigned long long>(boundaryCases),static_cast<unsigned long long>(randomCases),
            static_cast<unsigned long long>(largeCases),static_cast<unsigned long long>(wrapCases),
            static_cast<unsigned long long>(metrics.cases),static_cast<unsigned long long>(metrics.ticks),
            static_cast<unsigned long long>(metrics.comparisons),static_cast<unsigned long long>(metrics.referenceIncrements),
            static_cast<unsigned long long>(metrics.mmioAccesses),static_cast<unsigned long long>(allocations));
        std::fputs(report,stdout);
        if(argc>1){std::ofstream output(argv[1]);output<<report;check(bool(output),"Cannot save report");}
        return 0;
    }
    catch(const std::exception& e){countAllocations=false;std::fprintf(stderr,"FAIL: %s\n",e.what());return 1;}
}
