#pragma once
namespace ks {
// One native interrupt completion event. The scheduler supplies the admitted
// guest cost; saved interrupt state belongs to that scheduler, not this job.
class ExceptionReturnTask final {public:
 bool complete()const{return complete_;}
 unsigned nextClocks(unsigned returnCost)const{return complete_?0:returnCost;}
 unsigned advanceBefore(unsigned available,unsigned returnCost){if(complete_||returnCost>available)return 0;complete_=true;return returnCost;}
private:bool complete_=false;
};}
