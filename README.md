ccexif
======

Header only simple EXIF reader in C++

Based on https://code.google.com/p/easyexif/source/browse/trunk/

Sample
===

    #include "exif.h"
    #include <iostream>
    
    int main(int, const char *argv[])
    {
      std::string s = exif::extract_exif(argv[1]);
      if (s.empty())  return 0;
    
      exif::Info info;
      if (info.parse(s)) {
          std::cout << info.Make << "," << info.Model << "," << info.Software << ", " << info.DateTimeOriginal << std::endl;
      }
    
      return 0;
    }

License
===
BSD
