/* Copyright (C) 2015 Fimagena

   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public
   License as published by the Free Software Foundation; either
   version 2 of the License, or (at your option) any later version.

   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.

   You should have received a copy of the GNU Library General Public License
   along with this library; see the file COPYING.  If not, write to
   the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
   Boston, MA 02110-1301, USA.
*/

#pragma once

#include <memory>

#include "config.h"
#include <dng_image_writer.h>
#include <dng_date_time.h>
#include <dng_negative.h>
#include <dng_render.h>
#include <dng_preview.h>
#include <exiv2/image.hpp>
#include <libraw/libraw.h>


const char* getDngErrorMessage(int errorCode);

class NegativeProcessor {
public:
   static std::unique_ptr<LibRaw> getLibRaw(const char* filename) {
      std::unique_ptr<LibRaw> rawProcessor(std::make_unique<LibRaw>());

      int ret = rawProcessor->open_file(filename);
      if (ret != LIBRAW_SUCCESS) {
         rawProcessor->recycle();
         std::stringstream error; error << "LibRaw-error while opening rawFile: " << libraw_strerror(ret);
         throw std::runtime_error(error.str());
      }

      ret = rawProcessor->unpack();
      if (ret != LIBRAW_SUCCESS) {
         rawProcessor->recycle();
         std::stringstream error; error << "LibRaw-error while unpacking rawFile: " << libraw_strerror(ret);
         throw std::runtime_error(error.str());
      }

      return rawProcessor;
   }
   static std::unique_ptr<Exiv2::Image> getExivImage(const char* filename) {

      Exiv2::Image::UniquePtr rawImage;
      try {
         rawImage = Exiv2::ImageFactory::open(filename);
         rawImage->readMetadata();
      }
      catch (Exiv2::Error& e) {
         std::stringstream error; error << "Exiv2-error while opening/parsing rawFile (code " << static_cast<size_t>(e.code()) << "): " << e.what();
         throw std::runtime_error(error.str());
      }
      return rawImage;
   }
   static std::unique_ptr<NegativeProcessor> createProcessor(dng_host& host, const char *filename, const char* dcpFilename="");

   NegativeProcessor(dng_host& host, std::unique_ptr<dng_negative> negative);
   virtual ~NegativeProcessor();
   bool operator==(const NegativeProcessor& other) const;

   dng_preview_list* getPreview() { return m_previewList; }
   dng_negative& getNegative() { return *m_negative; }

   inline void rebuildIPTC(bool flag) { m_negative->RebuildIPTC(flag); }
   inline dng_metadata& getDngMetadata() { return m_negative->Metadata(); }
   inline dng_render getDngRender() { return {m_host, *m_negative}; }
   std::shared_ptr<dng_jpeg_preview> getJpegPreview();

   void renderPreviews();
   void renderImage();

protected:
   // Source: Raw-file
   std::unique_ptr<LibRaw> m_RawProcessor;
   Exiv2::Image::UniquePtr m_RawImage;
   Exiv2::ExifData m_RawExif;
   Exiv2::XmpData m_RawXmp;

   // Target: DNG-file
   dng_date_time_info m_dateTimeNow;
   dng_host& m_host;
   dng_preview_list* m_previewList{nullptr};
   std::unique_ptr<dng_negative> m_negative;
   std::string m_appName{"raw2dng"};
   std::string m_appVersion{RAW2DNG_VERSION_STR};
};
