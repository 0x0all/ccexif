#include "exif.h"
#include <iostream>

int main(int, const char *argv[])
{
  std::string s = exif::extract_exif(argv[1]);
  if (s.empty())  return 0;

  exif::Info info; 
  if (info.parse(s)) {
      std::cout << info.Make << "," << info.Model << "," << info.Software << ", " << info.DateTimeOriginal << ", " << info.FNumber << std::endl;
  }

  return 0;
}

