//LICENSE: BSD
//Based on: https://code.google.com/p/easyexif/source/browse/trunk/exif.{h,cpp}

#pragma once
#include <string>
#include <vector>
#include <fstream>

namespace exif
{
  enum Endian { INTEL = 0, MOTOROLA = 1 };
  enum Format {
    FORMAT_BYTE       =  1,
    FORMAT_ASCII      =  2,
    FORMAT_SHORT      =  3,
    FORMAT_LONG       =  4,
    FORMAT_RATIONAL   =  5,
    FORMAT_SBYTE      =  6,
    FORMAT_UNDEFINED  =  7,
    FORMAT_SSHORT     =  8,
    FORMAT_SLONG      =  9,
    FORMAT_SRATIONAL  = 10,
    FORMAT_FLOAT      = 11,
    FORMAT_DOUBLE     = 12
  };

  inline uint32_t to_uint32(const unsigned char buf[4], Endian endian = INTEL) {
    return endian == INTEL? 
        (uint32_t(buf[3] << 24) | (buf[2] << 16) | (buf[1] << 8) | buf[0]):
        (uint32_t(buf[0] << 24) | (buf[1] << 16) | (buf[2] << 8) | buf[3]); 
  } 

  inline uint16_t to_uint16(const unsigned char buf[2], Endian endian = INTEL) {
    return endian == INTEL? 
        (uint16_t(buf[1] << 8) | buf[0]):
        (uint16_t(buf[0] << 8) | buf[1]);
  }

  inline double to_rational(const unsigned char buf[8], Endian endian = INTEL) {
    double numerator = (double)to_uint32(buf, endian);
    double denominator = (double)to_uint32(buf + 4, endian);
    return (denominator > 1e-20? numerator / denominator : 0.);
  }

  inline std::string extract_exif(std::istream& is) {
    //SOI
    std::string nil;
    if (is.get() != 0xff || is.get() != 0xd8)
      return nil;

    if (is.get() != 0xff || !is.good())
      return nil;

    unsigned char c = is.get();
    //APP0
    if (c == 0xe0) {
      size_t length = (((unsigned int)is.get()) << 8) | (unsigned char)(is.get());
      if (! is.good() || length < 2) return nil;
      length -= 2;
      is.seekg(length, std::ios::cur);
      if (is.get() != 0xff || !is.good())
        return nil;
      c = is.get();
    }

    //APP1
    if (c == 0xe1) {
      size_t length = ((unsigned char)(is.get()) << 8) | (unsigned char)(is.get());
      if (! is.good()) return nil;

      std::vector<char> buf(length);
      is.read(buf.data(), length);
      if (! is.good()) return nil;

      return std::string(buf.data(), length);
    }

    return nil;
  }

  inline std::string extract_exif(const std::string& path) {
    std::ifstream ifs(path, std::ifstream::binary);
    return extract_exif(ifs);
  }

  struct Entry
  {
    uint16_t tag_;
    uint16_t format_;
    uint32_t length_;
    unsigned char data_[4];

    Endian endian_;

    Entry(const unsigned char buf[12], Endian endian = INTEL) { parse(buf, endian); }
    void parse(const unsigned char buf[12], Endian endian = INTEL) {
      tag_ = exif::to_uint16(buf, endian);
      format_ = exif::to_uint16(buf + 2, endian);
      length_ = exif::to_uint32(buf + 2 + 2, endian);
      *(uint32_t *)data_ = *(uint32_t *)(buf + 2 + 2 + 4);
      endian_ = endian;
    }

    uint8_t to_uint8() const { return data_[0]; }
    uint16_t to_uint16() const { return exif::to_uint16(data_, endian_); }
    uint32_t to_uint32() const { return exif::to_uint32(data_, endian_); }

    std::string to_string(const unsigned char * base, const unsigned char *end) const {
      if (length_ <= 4) 
        return std::string((const char *)data_, length_);

      uint32_t offset = to_uint32();
      if (base + offset + length_ <= end)
        return std::string((const char *)(base + offset), length_);

      return std::string();
    }

    double to_rational(const unsigned char *base, const unsigned char *end) const { 
      uint32_t offset = to_uint32();
      if (base + offset + 8 <= end) return exif::to_rational(base + offset, endian_);
      return 0; 
    }
  };

  struct Info
  {
    void reset() { *this = Info(); }
    bool parse(const std::string& buf) { return parse((const unsigned char *)buf.c_str(), buf.length()); }
    bool parse(const unsigned char *buf, size_t length) {
      size_t offset = 0;
      
      if (length < 6 + 8) {
        return false;
      }

      if (! std::equal(buf, buf + 6, "Exif\0\0")) {
        return false;
      }
      offset += 6;

// Now parsing the TIFF header. The first two bytes are either "II" or
// "MM" for Intel or Motorola byte alignment. Sanity check by parsing
// the unsigned short that follows, making sure it equals 0x2a. The
// last 4 bytes are an offset into the first IFD, which are added to 
// the global offset counter. For this block, we expect the following
// minimum size:
//  2 bytes: 'II' or 'MM'
//  2 bytes: 0x002a
//  4 bytes: offset to first IDF
// -----------------------------
//  8 bytes
      Endian endian = INTEL;
      if (buf[offset] == 'I' && buf[offset+1] == 'I') {
        endian = INTEL;
      }
      else if (buf[offset] == 'M' && buf[offset+1] == 'M') {
        endian = MOTOROLA;
      }
      else {
        return false;
      }

      endian_ = endian;

      if (to_uint16(buf+offset + 2, endian) != 0x2a) {
        return false;
      }
      uint32_t ifd_offset = to_uint32(buf+offset + 2 + 2, endian);
      offset += ifd_offset; 

// Now parsing the first Image File Directory (IFD0, for the main image).
// An IFD consists of a variable number of 12-byte directory entries. The
// first two bytes of the IFD section contain the number of directory
// entries in the section. The last 4 bytes of the IFD contain an offset
// to the next IFD, which means this IFD must contain exactly 6 + 12 * num
// bytes of data.
      if (length < offset + 2) {
        return false;
      }
     
      int n_entries = to_uint16(buf + offset, endian);
      if (length < offset + 2 + n_entries * 12 + 4) {
        return false;
      }
      offset += 2;

      const unsigned char *base = buf + 6, *end = buf + length;
      uint32_t gps_offset = length, exif_offset = length;

      while (--n_entries >= 0) {
        Entry entry(buf + offset, endian);
        offset += 12;

        switch(entry.tag_) {
        case 0x102: 
          // Bits per sample
          if (entry.format_ == FORMAT_SHORT) BitsPerSample = entry.to_uint16(); break;

        case 0x10E:
          // Image description
          if (entry.format_ == FORMAT_ASCII) ImageDescription = entry.to_string(base, end); break;

        case 0x10F:
          // Digicam make
          if (entry.format_ == FORMAT_ASCII) Make = entry.to_string(base, end); break;

        case 0x110:
          // Digicam model
          if (entry.format_ == FORMAT_ASCII) Model = entry.to_string(base, end); break;
  
        case 0x112:
          // Orientation of image
          if (entry.format_ == FORMAT_SHORT) Orientation = entry.to_uint16(); break;
  
        case 0x131:
          // Software used for image
          if (entry.format_ == FORMAT_ASCII) Software = entry.to_string(base, end); break;
  
        case 0x132:
          // EXIF/TIFF date/time of image modification
          if (entry.format_ == FORMAT_ASCII) DateTime = entry.to_string(base, end); break;
  
        case 0x8298:
          // Copyright information
          if (entry.format_ == FORMAT_ASCII) Copyright = entry.to_string(base, end); break;
  
        case 0x8825:
          // GPS IFD offset
          gps_offset = 6 + entry.to_uint32(); break;
  
        case 0x8769:
          // EXIF IFD offset
          exif_offset = 6 + entry.to_uint32(); break;

        default: break;
        }
      }

// Jump to the EXIF SubIFD if it exists and parse all the information
// there. Note that it's possible that the EXIF SubIFD doesn't exist.
// The EXIF SubIFD contains most of the interesting information that a
// typical user might want.
      if (exif_offset + 4 <= length) {
        offset = exif_offset;
        n_entries = to_uint16(buf + offset, endian);
        if (length < offset + 2 + n_entries * 12 + 4) {
          return false;
        }
        offset += 2;

        while (--n_entries >= 0) {
          Entry entry(buf + offset, endian);
          offset += 12;

          switch(entry.tag_) {
          case 0x829a:
            // Exposure time in seconds
            if (entry.format_ == FORMAT_RATIONAL) ExposureTime = entry.to_rational(base, end); break;
  
          case 0x829d:
            // FNumber
            if (entry.format_ == FORMAT_RATIONAL) FNumber = entry.to_rational(base, end); break;
  
          case 0x8827:
            // ISO Speed Rating
            if (entry.format_ == FORMAT_SHORT) ISOSpeedRatings = entry.to_uint16(); break;
  
          case 0x9003:
            // Original date and time
            if (entry.format_ == FORMAT_ASCII) DateTimeOriginal = entry.to_string(base, end); break;
  
          case 0x9004:
            // Digitization date and time
            if (entry.format_ == FORMAT_ASCII) DateTimeDigitized = entry.to_string(base, end); break;
  
          case 0x9201:
            // Shutter speed value
            if (entry.format_ == FORMAT_RATIONAL) ShutterSpeedValue = entry.to_rational(base, end); break;

          case 0x9202:
            // Shutter speed value
            if (entry.format_ == FORMAT_RATIONAL) ApertureValue = entry.to_rational(base, end); break;
  
          case 0x9204:
            // Exposure bias value 
            if (entry.format_ == FORMAT_RATIONAL) ExposureBiasValue = entry.to_rational(base, end); break;
  
          case 0x9206:
            // Subject distance
            if (entry.format_ == FORMAT_RATIONAL) SubjectDistance = entry.to_rational(base, end); break;
  
          case 0x9209:
            // Flash used
            if (entry.format_ == FORMAT_SHORT) Flash = entry.to_uint16()? 1 : 0; break;
  
          case 0x920a:
            // Focal length
            if (entry.format_ == FORMAT_RATIONAL) FocalLength = entry.to_rational(base, end); break;
  
          case 0x9207:
            // Metering mode
            if (entry.format_ == FORMAT_SHORT) MeteringMode = entry.to_uint16(); break;
  
          case 0x9291:
            // Subsecond original time
            if (entry.format_ == FORMAT_ASCII) SubSecTimeOriginal = entry.to_string(base, end); break;
  
          case 0xa002:
            // EXIF Image width
            if (entry.format_ == FORMAT_LONG) ImageWidth = entry.to_uint32();
            else if (entry.format_ == FORMAT_SHORT) ImageWidth = entry.to_uint16();
            break;
  
          case 0xa003:
            // EXIF Image height
            if (entry.format_ == FORMAT_LONG) ImageHeight = entry.to_uint32();
            else if (entry.format_ == FORMAT_SHORT) ImageHeight = entry.to_uint16();
            break;
  
          case 0xa405:
            // Focal length in 35mm film
            if (entry.format_ == FORMAT_SHORT) FocalLengthIn35mm = entry.to_uint16(); break;

          default:
            break;
          }
        }
      } // EXIF

// Jump to the GPS SubIFD if it exists and parse all the information
// there. Note that it's possible that the GPS SubIFD doesn't exist.
      if (gps_offset + 4 <= length) {
        offset = gps_offset;
        n_entries = to_uint16(buf + offset, endian);
        if (length < offset + 2 + n_entries * 12 + 4) {
          return false;
        }
        offset += 2;

        while (--n_entries >= 0) {
          Entry entry(buf + offset, endian);
          offset += 12;

          switch(entry.tag_) {
            case 1:
              // GPS north or south
              GeoLocation.LatComponents.direction = entry.to_uint8(); break;
    
            case 2:
              // GPS latitude
              if (entry.format_ == FORMAT_RATIONAL && entry.length_ == 3) {
                GeoLocation.LatComponents.degrees = entry.to_rational(base, end);
                GeoLocation.LatComponents.minutes = entry.to_rational(base + 8, end);
                GeoLocation.LatComponents.seconds = entry.to_rational(base + 16, end);
                GeoLocation.Latitude = GeoLocation.LatComponents.to_rational();
              }
              break;
    
            case 3:
              // GPS east or west
              GeoLocation.LonComponents.direction = entry.to_uint8(); break;
    
            case 4:
              // GPS longitude
              if (entry.format_ == FORMAT_RATIONAL && entry.length_ == 3) {
                GeoLocation.LonComponents.degrees = entry.to_rational(base, end);
                GeoLocation.LonComponents.minutes = entry.to_rational(base + 8, end);
                GeoLocation.LonComponents.seconds = entry.to_rational(base + 16, end);
                GeoLocation.Longitude = GeoLocation.LonComponents.to_rational();
              }
              break;
    
            case 5:
              // GPS altitude reference (below or above sea level)
              GeoLocation.AltitudeRef = entry.to_uint8(); break; break;
    
            case 6:
              // GPS altitude reference
              if (entry.format_ == FORMAT_RATIONAL) GeoLocation.Altitude = entry.to_rational(base, end); break;          
          
          default:
            break;
          }
        }

        if ('S' == GeoLocation.LatComponents.direction) GeoLocation.Latitude = -GeoLocation.Latitude;
        if ('W' == GeoLocation.LonComponents.direction) GeoLocation.Longitude = -GeoLocation.Longitude;
        if (1 == GeoLocation.AltitudeRef) GeoLocation.Altitude = -GeoLocation.Altitude;
      }

      return true;
    } // parse()

    Endian endian_;

    std::string ImageDescription;     // Image description
    std::string Make;                 // Camera manufacturer's name
    std::string Model;                // Camera model
    unsigned short Orientation;       // Image orientation, start of data corresponds to
                                      // 0: unspecified in EXIF data
                                      // 1: upper left of image
                                      // 3: lower right of image
                                      // 6: upper right of image
                                      // 8: lower left of image
                                      // 9: undefined
    unsigned short BitsPerSample;     // Number of bits per component
    std::string Software;             // Software used
    std::string DateTime;             // File change date and time
    std::string SubSecTimeOriginal;   // Sub-second time that original picture was taken
    std::string Copyright;            // File copyright information

    //EXIF
    // http://www.awaresystems.be/imaging/tiff/tifftags/privateifd/exif.html
    double ExposureTime;              // Exposure time in seconds
    double FNumber;                   // F/stop
    unsigned short ExposureProgram;   // The class of the program used by the camera to set exposure when the picture is taken.
                                      // 0 = Not defined
                                      // 1 = Manual
                                      // 2 = Normal program
                                      // 3 = Aperture priority
                                      // 4 = Shutter priority
                                      // 5 = Creative program (biased toward depth of field)
                                      // 6 = Action program (biased toward fast shutter speed)
                                      // 7 = Portrait mode (for closeup photos with the background out of focus)
                                      // 8 = Landscape mode (for landscape photos with the background in focus)
//  std::string SpectralSensitivity;
    unsigned short ISOSpeedRatings;   // ISO speed
    std::string DateTimeOriginal;     // Original file date and time (may not exist)
    std::string DateTimeDigitized;    // Digitization date and time (may not exist)
    double ShutterSpeedValue;         // Shutter speed (reciprocal of exposure time)
    double ApertureValue;             // The lens aperture.
    double ExposureBiasValue;         // Exposure bias value in EV
    double MaxApertureValue;          // The smallest F number of the lens.
    double SubjectDistance;           // Distance to focus point in meters
    unsigned short MeteringMode;      // Metering mode
                                      // 1: average
                                      // 2: center weighted average
                                      // 3: spot
                                      // 4: multi-spot
                                      // 5: multi-segment
    char Flash;                       // 0 = no flash, 1 = flash used
    double FocalLength;               // Focal length of lens in millimeters
    unsigned short FocalLengthIn35mm; // Focal length in 35mm film
    unsigned int ImageWidth;              // Image width reported in EXIF data
    unsigned int ImageHeight;             // Image height reported in EXIF data

    //GPS
    // http://www.awaresystems.be/imaging/tiff/tifftags/privateifd/gps.html
    struct Geolocation_t {            // GPS information embedded in file
      double Latitude;                  // Image latitude expressed as decimal
      double Longitude;                 // Image longitude expressed as decimal
      double Altitude;                  // Altitude in meters, relative to sea level
      char AltitudeRef;                 // 0 = above sea level, -1 = below sea level
      struct Coord_t {
        double degrees;               
        double minutes;
        double seconds;
        char direction;
        double to_rational() const { return degrees + minutes / 60 + seconds / 3600; }
      } LatComponents, LonComponents;   // Latitude, Longitude expressed in deg/min/sec 
    } GeoLocation; 
  };
};

