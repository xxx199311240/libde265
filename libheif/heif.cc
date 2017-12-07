
#include "box.h"

#include <fstream>
#include <iostream>

int main(int argc, char** argv)
{
  using heif::BoxHeader;
  using heif::Box;

  std::ifstream istr(argv[1]);

  for (;;) {
    uint64_t maxSize = std::numeric_limits<uint64_t>::max();
    auto box = Box::read(istr, maxSize);

    if (istr.fail()) {
      break;
    }

    std::cout << "\n";
    std::cout << box->dump();
  }
}
