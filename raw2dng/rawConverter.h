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
#include <string>

#include "negativeProcessor.h"
#include "dng_auto_ptr.h"
#include "dng_string.h"
#include "dng_image_writer.h"
#include "dng_preview.h"
#include "dng_date_time.h"


class RawConverter {
public:
   RawConverter();
   virtual ~RawConverter();

   void openRawFile(const std::string& rawFilename);
   void buildNegative(const std::string& dcpFilename);
   void embedRaw(const std::string& rawFilename);
   void renderImage();
   void renderPreviews();

   void writeDng (const std::string& outFilename);
   void writeTiff(const std::string& outFilename);
   void writeJpeg(const std::string& outFilename);

private:
   std::unique_ptr<dng_host> m_host;
   std::unique_ptr<NegativeProcessor> m_negProcessor;
   dng_preview_list* m_previewList;
   dng_string m_appName, m_appVersion;
   dng_date_time_info m_dateTimeNow;

};
