
#pragma once

#include "BaseProcessor.h"


class ILCE7processor : public BaseProcessor<ILCE7processor> {
   friend class BaseProcessor<ILCE7processor>;

public:
   void setDNGPropertiesFromRaw();
   void setExifFromRaw(const dng_date_time_info& dateTimeNow,
                       const dng_string&         appNameVersion);
   void setXmpFromRaw(const dng_date_time_info& dateTimeNow,
                      const dng_string&         appNameVersion);

   // Shadows BaseProcessor::createDNGPrivateTag to append the Sony SR2-IFD
   // blob after the standard Adobe MakerNote block.
   dng_memory_stream* createDNGPrivateTag();

protected:
   ILCE7processor(dng_host&                    host,
                  std::unique_ptr<LibRaw>      rawProcessor,
                  Exiv2::Image::UniquePtr      rawImage);
};