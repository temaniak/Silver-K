#include "ksFirmwareLayoutCheckTask.h"
#include <stdexcept>
#include <iostream>
int main(){using T=ks::FirmwareLayoutCheckTask;unsigned cases=0;for(unsigned budget=0;budget<256;++budget){T t;unsigned used=0;while(t.stage()!=T::Stage::Complete){auto cost=t.nextClocks();if(t.advanceBefore(cost-1))throw std::runtime_error("early budget");used+=t.advanceBefore(budget<cost?cost:budget);}if(used!=38||t.extent()!=454||t.advanceBefore(65535))throw std::runtime_error("layout extent/clocks/completion");++cases;}std::cout<<"PASS: "<<cases<<" pure firmware-layout-check properties\n";}
