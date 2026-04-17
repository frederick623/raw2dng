
#pragma once

#include "BaseProcessor.h"


// TODO/FIXME: Fuji support is currently broken (buildDNGImage rotation).

class FujiProcessor : public BaseProcessor<FujiProcessor> {
   friend class BaseProcessor<FujiProcessor>;

public:
   void setDNGPropertiesFromRaw();
   void buildDNGImage();

protected:
   FujiProcessor(dng_host&                    host,
                 std::unique_ptr<LibRaw>      rawProcessor,
                 Exiv2::Image::UniquePtr      rawImage);

   bool m_fujiRotate90;
};