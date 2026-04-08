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

#include "rawConverter.h"

#include <algorithm>
#include <stdexcept>

#include "dng_negative.h"
#include "dng_image_writer.h"
#include "dng_preview.h"
#include "dng_memory_stream.h"
#include "dng_file_stream.h"
#include "dng_render.h"
#include "dng_simple_image.h"
#include "dng_color_space.h"
#include "dng_exceptions.h"
#include "dng_tag_values.h"
#include "dng_tag_codes.h"
#include "dng_xmp.h"
#include "dng_color_space.h"

#include "negativeProcessor.h"

#include "util.hpp"

RawConverter::RawConverter(const std::vector<std::string>& rawFilenames, const std::string& dcpFilename)
{
    // -----------------------------------------------------------------------------------------
    // Init XMP SDK and some global variables we will need

    m_host.SetSaveDNGVersion(dngVersion_SaveDefault);
    m_host.SetSaveLinearDNG(false);
    m_host.SetKeepOriginalFile(true);

    for (const auto& rawFilename : rawFilenames)
    {
        auto pair = m_negProcessors.insert_or_assign(rawFilename
            , NegativeProcessor::createProcessor(m_host, rawFilename.c_str()));
        pair.first->second->setCameraProfile(dcpFilename.c_str());
        // -----------------------------------------------------------------------------------------
        // Copy raw sensor data
        pair.first->second->buildDNGImage();
        pair.first->second->renderImage();
        pair.first->second->renderPreviews();
    }
}


void RawConverter::writeDng(const std::string inFilename, const std::string& outFilename) {
    // -----------------------------------------------------------------------------------------
    // Write DNG-image to file

    try {
        if (!m_negProcessors.contains(inFilename)) return;

        auto& m_negProcessor = m_negProcessors[inFilename];
        auto targetFile = dng_file_stream(outFilename.c_str(), true);
        dng_image_writer().WriteDNG(m_host, targetFile, m_negProcessor->getNegative(), m_negProcessor->getPreview());
    }
    catch (dng_exception& e) {
        std::stringstream error; error << "Error while writing DNG-file! (" << e.ErrorCode() << ": " << getDngErrorMessage(e.ErrorCode()) << ")";
        throw std::runtime_error(error.str());
    }
}

std::string RawConverter::merge(const std::unordered_map<std::string, double>& inputs)
{
    if (m_negProcessors.size()<=1 or m_negProcessors.size()!=inputs.size() or 
        std::any_of(std::next(m_negProcessors.begin()), m_negProcessors.end(), [&](const auto& pair){
            return m_negProcessors.begin()->second!=pair.second;
        }))
        return "";

    const dng_image *reference = m_negProcessors.begin()->second->getNegative().Stage1Image();
    const dng_rect bounds = reference->Bounds();
    const uint32 planes = reference->Planes();

    auto *mergedImage = new dng_simple_image(bounds, planes, ttShort, m_host.Allocator());
    dng_pixel_buffer mergedBuffer;
    mergedImage->GetPixelBuffer(mergedBuffer);

    auto width = bounds.W();
    const auto samplesPerRow = width * planes;
    std::vector<uint32_t> accumulator(samplesPerRow);

    for (int32 row = bounds.t; row < bounds.b; ++row) {
        std::fill(accumulator.begin(), accumulator.end(), 0U);

        for (const auto &input : inputs) {
            auto& negative = m_negProcessors[input.first]->getNegative();
            auto image = negative.Stage1Image();
            dng_pixel_buffer sourceBuffer;
            dynamic_cast<const dng_simple_image*>(image)->GetPixelBuffer(sourceBuffer);
            const uint16 *source = sourceBuffer.ConstPixel_uint16(row, bounds.l);
            const double gain = std::exp2(input.second);

            for (uint32 col = 0; col < width; ++col) {
                for (uint32 plane = 0; plane < planes; ++plane) {
                    const uint32 sample = col * planes + plane;
                    const double black = negative.RawImageBlackLevel();
                    const double white = negative.WhiteLevel(plane);
                    const double value = static_cast<double>(source[sample]);
                    const double scaled = std::clamp((value - black) * gain + black, black, white);
                    accumulator[sample] += static_cast<uint32_t>(std::lround(scaled));
                }
            }
        }

        uint16 *destination = mergedBuffer.DirtyPixel_uint16(row, bounds.l);
        for (uint32 sample = 0; sample < samplesPerRow; ++sample) {
            destination[sample] = static_cast<uint16>((accumulator[sample] + inputs.size() / 2) / inputs.size());
        }
    }

    for (auto it=std::next(m_negProcessors.begin()); it!=m_negProcessors.end(); ++it)
    {
        
    }

    return m_negProcessors.begin()->first;
}


void RawConverter::writeTiff(const std::string inFilename, const std::string& outFilename) {
    // -----------------------------------------------------------------------------------------
    // Write Tiff-image to file

    try {
        if (!m_negProcessors.contains(inFilename)) return;

        auto& m_negProcessor = m_negProcessors[inFilename];
        // -----------------------------------------------------------------------------------------
        // Render TIFF

        dng_render negRender(m_negProcessor->getDngRender());
        std::unique_ptr<dng_image> negImage(negRender.Render());

        auto targetFile = dng_file_stream(outFilename.c_str(), true);
        dng_image_writer().WriteTIFF(m_host, targetFile, *negImage, piRGB, ccUncompressed,
                             &m_negProcessor->getDngMetadata(), &dng_space_sRGB::Get(), NULL,
                             dynamic_cast<const dng_jpeg_preview*>(&m_negProcessor->getPreview()->Preview(1)));
    }
    catch (dng_exception& e) {
        std::stringstream error; error << "Error while writing TIFF-file! (" << e.ErrorCode() << ": " << getDngErrorMessage(e.ErrorCode()) << ")";
        throw std::runtime_error(error.str());
    }
}

void RawConverter::updateMetadata(dng_negative &negative, dng_host &host, std::size_t inputCount) {
    dng_date_time_info now;
    CurrentDateTimeAndZone(now);
    negative.UpdateDateTime(now);

    if (dng_exif *exif = negative.GetExif()) {
        exif->fSoftware.Set_ASCII("raw2dng dngmerge");
        exif->fImageDescription.Set_ASCII(("Merged " + std::to_string(inputCount) + " DNG exposures").c_str());
    }

    if (dng_xmp *xmp = negative.GetXMP()) {
        xmp->UpdateDateTime(now);
        xmp->UpdateMetadataDate(now);
        xmp->SetString(XMP_NS_XAP, "CreatorTool", "raw2dng dngmerge");
        xmp->Set(XMP_NS_DC, "format", "image/dng");
        xmp->SetString(XMP_NS_DC, "description", ("Merged " + std::to_string(inputCount) + " DNG exposures").c_str());
    }

    negative.SetOriginalRawFileName("merged.dng");
    negative.SetHasOriginalRawFileData(false);
    AutoPtr<dng_memory_block> emptyOriginal;
    negative.SetOriginalRawFileData(emptyOriginal);
    negative.ClearRawImageDigest();
    negative.FindRawImageDigest(host);
    negative.SynchronizeMetadata();
}


void RawConverter::writeJpeg(const std::string inFilename, const std::string& outFilename) {
    // -----------------------------------------------------------------------------------------
    // Render JPEG

    // FIXME: we should render and integrate a thumbnail too

    if (!m_negProcessors.contains(inFilename)) return;

    auto& m_negProcessor = m_negProcessors[inFilename];

    dng_render negRender(m_negProcessor->getDngRender());
    std::unique_ptr<dng_image> negImage(negRender.Render());

    auto jpeg = m_negProcessor->getJpegPreview();
    dng_image_writer().EncodeJPEGPreview(m_host, *negImage, *jpeg, 8);

    // -----------------------------------------------------------------------------------------
    // Write JPEG-image to file

    const uint8 soiTag[]         = {0xff, 0xd8};
    const uint8 app1Tag[]        = {0xff, 0xe1};
    const char* app1ExifHeader   = "Exif\0";
    const int exifHeaderLength   = 6;
    const uint8 tiffHeader[]     = {0x49, 0x49, 0x2a, 0x00, 0x08, 0x00, 0x00, 0x00};
    const char* app1XmpHeader    = "http://ns.adobe.com/xap/1.0/";
    const int xmpHeaderLength    = 29;
    const char* app1ExtXmpHeader = "http://ns.adobe.com/xmp/extension/";
    const int extXmpHeaderLength = 35;
    const int jfifHeaderLength   = 20;

    // hack: we're overloading the class just to get access to protected members (DNG-SDK doesn't exposure full Put()-function on these)
    class ExifIfds : public exif_tag_set {
    public:
        dng_tiff_directory* getExifIfd() {return &fExifIFD;}
        dng_tiff_directory* getGpsIfd() {return &fGPSIFD;}
        ExifIfds(dng_tiff_directory &directory, const dng_exif &exif, dng_metadata* md) :
            exif_tag_set(directory, exif, md->IsMakerNoteSafe(), md->MakerNoteData(), md->MakerNoteLength(), false) {}
    };

    try {
        // -----------------------------------------------------------------------------------------
        // Build IFD0, ExifIFD, GPSIFD
        auto targetFile = dng_file_stream(outFilename.c_str(), true);

        dng_metadata* metadata = &m_negProcessor->getDngMetadata();
        metadata->GetXMP()->Set(XMP_NS_DC, "format", "image/jpeg");

        dng_tiff_directory mainIfd;

        tag_uint16 tagOrientation(tcOrientation, metadata->BaseOrientation().GetTIFF());
        mainIfd.Add(&tagOrientation);

        // this is non-standard I believe but let's leave it anyway
        tag_iptc tagIPTC(metadata->IPTCData(), metadata->IPTCLength());
        if (tagIPTC.Count()) mainIfd.Add(&tagIPTC);

        // this creates exif and gps Ifd and also adds the following to mainIfd:
        // datetime, imagedescription, make, model, software, artist, copyright, exifIfd, gpsIfd
        ExifIfds exifSet(mainIfd, *metadata->GetExif(), metadata);
        exifSet.Locate(sizeof(tiffHeader) + mainIfd.Size());

        // we're ignoring YCbCrPositioning, XResolution, YResolution, ResolutionUnit
        // YCbCrCoefficients, ReferenceBlackWhite

        // -----------------------------------------------------------------------------------------
        // Build IFD0, ExifIFD, GPSIFD

        // Write SOI-tag
        targetFile.Put(soiTag, sizeof(soiTag));

        // Write APP1-Exif section: Header...
        targetFile.Put(app1Tag, sizeof(app1Tag));
        targetFile.SetBigEndian(true);
        targetFile.Put_uint16(sizeof(uint16) + exifHeaderLength + sizeof(tiffHeader) + mainIfd.Size() + exifSet.Size());
        targetFile.Put(app1ExifHeader, exifHeaderLength);

        // ...and TIFF structure
        targetFile.SetLittleEndian(true);
        targetFile.Put(tiffHeader, sizeof(tiffHeader));
        mainIfd.Put(targetFile, dng_tiff_directory::offsetsRelativeToExplicitBase, sizeof(tiffHeader));
        exifSet.getExifIfd()->Put(targetFile, dng_tiff_directory::offsetsRelativeToExplicitBase, sizeof(tiffHeader) + mainIfd.Size());
        exifSet.getGpsIfd()->Put(targetFile, dng_tiff_directory::offsetsRelativeToExplicitBase, sizeof(tiffHeader) + mainIfd.Size() + exifSet.getExifIfd()->Size());

        // Write APP1-XMP if required
        if (metadata->GetXMP()) {
            AutoPtr<dng_memory_block> stdBlock, extBlock;
            dng_string extDigest;
            metadata->GetXMP()->PackageForJPEG(stdBlock, extBlock, extDigest);

            targetFile.Put(app1Tag, sizeof(app1Tag));
            targetFile.SetBigEndian(true);
            targetFile.Put_uint16(sizeof(uint16) + xmpHeaderLength + stdBlock->LogicalSize());
            targetFile.Put(app1XmpHeader, xmpHeaderLength);
            targetFile.Put(stdBlock->Buffer(), stdBlock->LogicalSize());

            if (extBlock.Get()) {
                // we only support one extended block, if XMP is >128k the file will probably be corrupted
                targetFile.Put(app1Tag, sizeof(app1Tag));
                targetFile.SetBigEndian(true);
                targetFile.Put_uint16(sizeof(uint16) + extXmpHeaderLength + extDigest.Length() + sizeof(uint32) + sizeof(uint32) + extBlock->LogicalSize());
                targetFile.Put(app1ExtXmpHeader, extXmpHeaderLength);
                targetFile.Put(extDigest.Get(), extDigest.Length());
                targetFile.Put_uint32(extBlock->LogicalSize());
                targetFile.Put_uint32(stdBlock->LogicalSize());
                targetFile.Put(extBlock->Buffer(), extBlock->LogicalSize());
            }
        }

        // write remaining JPEG structure/data from libjpeg minus the JFIF-header
        targetFile.Put((uint8*) jpeg->CompressedData().Buffer() + jfifHeaderLength, jpeg->CompressedData().LogicalSize() - jfifHeaderLength);

        targetFile.Flush();
    }
    catch (dng_exception& e) {
        std::stringstream error; error << "Error while writing JPEG-file! (" << e.ErrorCode() << ": " << getDngErrorMessage(e.ErrorCode()) << ")";
        throw std::runtime_error(error.str());
    }
}
