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
#include <unordered_map>
#include <vector>

class NegativeProcessor;
class dng_host;
class dng_negative;

class RawConverter {
public:

   RawConverter(const std::vector<std::string>& rawFilenames, const std::string& dcpFilename);
   virtual ~RawConverter(); // defined in .cpp where DngHost is complete

   std::string merge(const std::unordered_map<std::string, double>& inputs);
   static void updateMetadata(dng_negative &negative, dng_host &host, std::size_t inputCount);
   void writeDng (const std::string inFilename, const std::string& outFilename);
   void writeTiff(const std::string inFilename, const std::string& outFilename);
   void writeJpeg(const std::string inFilename, const std::string& outFilename);

private:
   std::unordered_map<std::string, std::unique_ptr<NegativeProcessor>> m_negProcessors;
   std::unique_ptr<dng_host> m_host;

};
