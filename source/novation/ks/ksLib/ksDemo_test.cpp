#include "ksDemo.h"
#include "ksBoard.h"
#include <algorithm>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <stdexcept>

namespace {
void check(bool ok,const char* message){if(!ok)throw std::runtime_error(message);}
std::vector<uint8_t> bytes(const char* path){std::ifstream f(path,std::ios::binary);return {(std::istreambuf_iterator<char>(f)),{}};}
void song(std::vector<uint8_t>& flash){
 // Synthetic test song: generated-wave Note On, four beats, Note Off, EOT.
 const uint8_t midi[]{'M','T','h','d',0,0,0,6,0,0,0,1,0,96,'M','T','r','k',0,0,0,13,
                     0,0x90,60,100,0x83,0,0x80,60,0,0,0xff,0x2f,0};
 std::copy(std::begin(midi),std::end(midi),flash.begin()+0x1a000);
}
}
int main(int argc,char** argv){try{
 std::vector<uint8_t> flash(0x80000,255);check(!ks::validDemoSong(flash.data(),flash.size()),"Erased song accepted");
 song(flash);check(ks::validDemoSong(flash.data(),flash.size()),"Test song rejected");
 auto bad=flash;bad[0x1a00d]=0;check(!ks::validDemoSong(bad.data(),bad.size()),"Zero division accepted");
 bad=flash;bad[0x1a015]=255;check(!ks::validDemoSong(bad.data(),bad.size()),"Truncated track accepted");
 bad=flash;bad[0x1a020]=0x90;check(!ks::validDemoSong(bad.data(),bad.size()),"Missing EOT accepted");
 std::cout<<"PASS: demo data validation, erased/truncated/malformed data rejected\n"<<std::flush;
 if(argc==1)return 0;if(argc!=3)return 2;
 const auto os=bytes(argv[1]);flash=bytes(argv[2]);song(flash);std::vector<uint8_t> wave(ks::Board::WaveSize,0);
 for(unsigned slot=0;slot<50;++slot)std::copy_n(flash.data()+0x20000,256,flash.data()+0x20000+(500+slot)*256);
 auto board=std::make_unique<ks::Board>(os,wave,flash,true,true);
 check(board->boot(),"Native reset failed");check(board->demoAvailable(),"Demo unavailable");
 auto render=[&](unsigned count){for(unsigned n=0;n<count;++n)board->renderSample();};
 const auto before=board->state();
 check(board->setDemoSong(true),"Start rejected");
 bool playing=false;for(unsigned n=0;n<100 && !playing;++n){render(512);playing=board->read(0x1678c)!=0;}
 check(playing,"Panel chord did not start original demo player");
 std::cout<<"PASS: native panel MENUS + 1/2 started firmware demo player\n"<<std::flush;
 check(board->setDemoSong(false),"Stop rejected");render(24000);
 check(!board->demoActive(),"PROG did not stop demo");check(board->state().performance==before.performance,"Stop did not restore mode");
 check(board->setDemoSong(true),"Restart rejected");
 int peak=0;for(unsigned n=0;n<180000;++n){const auto frame=board->renderSample();peak=std::max(peak,std::max(std::abs(frame[0]),std::abs(frame[1])));}
 check(peak>1000,"Synthetic demo notes produced no audio");
 check(!board->demoActive(),"EOT did not stop demo");check(board->state().performance==before.performance,"EOT did not restore mode");
 const uint8_t multi[]{0xb0,32,65,0xc0,0};check(board->enqueueMidi(multi,sizeof(multi)),"Performance selection rejected");render(16000);check(board->state().performance,"Performance not selected");
 check(board->setDemoSong(true),"Performance demo rejected");render(20000);check(board->setDemoSong(false),"Performance stop rejected");render(24000);check(board->state().performance && !board->demoActive(),"Performance not restored after stop");
 const uint8_t kit[]{0xb0,32,39,0xc0,0};check(board->enqueueMidi(kit,sizeof(kit)),"Drum selection rejected");render(16000);
 const auto initialCutoff=board->state().parts[0][46];const uint8_t edit[]{0xb0,105,31};check(board->enqueueMidi(edit,sizeof(edit)),"Drum edit rejected");
 check(board->selectDrumNote(0,37),"Drum Edit key rejected");render(24000);
 check(!board->drumEditBusy() && board->state().parts[0][188]==1 && board->state().parts[0][46]==initialCutoff,"Native Drum Edit did not select next sound");
 check(board->selectDrumNote(0,36),"Drum Edit return rejected");render(24000);
 check(!board->drumEditBusy() && board->state().parts[0][46]==31,"Native Drum Edit lost queued control change");
 check(board->h8InterpretedSteps()==0,"Demo/Drum Edit used hidden H8 fallback");
 std::cout<<"PASS: native Drum Edit preserves queued knob edit across key changes; zero H8 interpreter steps\n";
 std::cout<<"PASS: stop, restart, EOT, original mode restored; zero H8 interpreter steps\n";
 return 0;
}catch(const std::exception& e){std::cerr<<"FAIL: "<<e.what()<<"\n";return 1;}}
