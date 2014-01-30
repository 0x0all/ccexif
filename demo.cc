#include "exif.h"
#include <iostream>

int main(int, const char *argv[])
{
  std::ifstream is(argv[1]);
  std::string s = exif::extract_exif(is);
  if (s.empty())  return 0;

  exif::Info info; 
  if (info.parse(s)) {
      std::cout << info.Make << "," << info.Model << "," << info.Software << ", " << info.DateTimeOriginal << std::endl;
  }

  return 0;
}
