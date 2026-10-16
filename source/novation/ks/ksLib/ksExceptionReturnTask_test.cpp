#include "ksExceptionReturnTask.h"
#include <stdexcept>
#include <iostream>
int main(){unsigned cases=0;for(unsigned cost=1;cost<=64;++cost)for(unsigned budget=0;budget<=128;++budget){ks::ExceptionReturnTask t;if(t.nextClocks(cost)!=cost)throw std::runtime_error("initial event");auto used=t.advanceBefore(budget,cost);if(used!=(budget>=cost?cost:0)||t.complete()!=(budget>=cost))throw std::runtime_error("budget admission");if(!t.complete()&&t.advanceBefore(cost,cost)!=cost)throw std::runtime_error("late admission");if(!t.complete()||t.advanceBefore(128,cost)||t.nextClocks(cost))throw std::runtime_error("repeat event");++cases;}std::cout<<"PASS: "<<cases<<" pure exception-return properties\n";}
