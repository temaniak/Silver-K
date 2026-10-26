#include "ksBoardComparisonBoundary.h"
#include <stdexcept>
namespace ks {
namespace {BoardComparisonFactory factory=nullptr;}
void setBoardComparisonFactoryForTests(BoardComparisonFactory value){factory=value;}
bool boardComparisonAvailable()noexcept{return factory!=nullptr;}
std::unique_ptr<BoardComparisonBackend> createBoardComparison(Board* board,const std::vector<uint8_t>&rom){
 if(!factory)throw std::logic_error("KS comparison backend unavailable");
 auto backend=factory(board,rom);if(!backend)throw std::logic_error("KS comparison factory returned no backend");return backend;
}
}
