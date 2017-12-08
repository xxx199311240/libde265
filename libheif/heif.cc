
#include "box.h"

#include <fstream>
#include <iostream>

int main(int argc, char** argv)
{
  using heif::BoxHeader;
  using heif::Box;

  std::ifstream istr(argv[1]);

  uint64_t maxSize = std::numeric_limits<uint64_t>::max();
  heif::BitstreamRange range(&istr, maxSize);

  for (;;) {
    auto box = Box::read(range);

    if (!box || range.error()) {
      break;
    }

    std::cout << "\n";
    std::cout << box->dump();
  }
}
