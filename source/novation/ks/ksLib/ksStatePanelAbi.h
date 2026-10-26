#pragma once
#include "ksStatePanelService.h"
namespace ks {
template<class A>void projectStatePanel(A&a,const StatePanelService&s){if(!s.complete())throw std::logic_error("Incomplete keyboard/control drain projection");if(s.biasObserved())a.values[0]=(a.values[0]&0xffffff00u)|s.bias();a.values[1]=s.controlAddress();a.values[6]=(a.values[6]&0xffff0000u)|s.event();a.flags=uint8_t((a.flags&0xd0)|4);}
}
