#pragma once
#include <cstdint>
namespace ks::midiFileTransportFixture {
// A finite file and genuine keyboard/control queue producers. The caller still
// supplies real ROM-backed schemas and the patch/part/voice fixture.
template<class Byte,class Word>
void finiteFile(Byte&&byte,Word&&word,unsigned exitMode=0,unsigned voices=0){
 word(0x81a00c,96);byte(0x81a016,exitMode?1:0);byte(0x81a017,255);byte(0x81a018,47);byte(0x81a019,0);
 for(unsigned n=0;n<21;++n)byte(0x81fe00+n,0);byte(0x81fe07,uint8_t(voices));
 if(exitMode==1){word(0x1537c,2);word(0x1537e,0);word(0x15380,0x0104);}
 if(exitMode==2){word(0x1537c,0);word(0x1537e,0);word(0x15406,2);word(0x15408,0);word(0x1540a,0x011b);}
}
template<class Word>
void laterExitKey(Word&&word){word(0x1537c,2);word(0x1537e,0);word(0x15380,0x0104);}
}
