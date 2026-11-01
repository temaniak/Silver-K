#pragma once
#include "ksDiagnosticTransferService.h"
namespace ks {
template<class A>void projectDiagnosticTransfer(A&a,const DiagnosticTransferService&s){if(!s.complete())throw std::logic_error("Incomplete diagnostic transfer");using K=DiagnosticTransferService::Kind;const auto&r=s.result();auto&v=a.values;v[1]=0;v[0]=(v[0]&0xffff0000u)|0xf700;
if(s.kind()==K::Block||s.kind()==K::Dump){v[2]=(uint32_t(r.encoded.carry)<<16)|r.encoded.weight;v[3]=r.encoded.sum;v[6]=r.source;if(s.kind()==K::Dump){v[0]=(r.progress&0xffff0000u)|0xf700;v[4]=r.block;v[5]=r.sum;}else v[0]=0xf700;}
a.flags=uint8_t((a.flags&0xd0)|8);}
}
