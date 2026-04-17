
#pragma once

#include "BaseProcessor.h"


class VariousVendorProcessor : public BaseProcessor<VariousVendorProcessor> {
   friend class BaseProcessor<VariousVendorProcessor>;

public:
   void setDNGPropertiesFromRaw();
   void setExifFromRaw(const dng_date_time_info& dateTimeNow,
                       const dng_string&         appNameVersion);

protected:
   VariousVendorProcessor(dng_host&                    host,
                           std::unique_ptr<LibRaw>      rawProcessor,
                           Exiv2::Image::UniquePtr      rawImage);
};