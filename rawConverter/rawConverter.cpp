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

#include "config.h"
#include "rawConverter.h"

#include <stdexcept>

#include "dng_date_time.h"
#include "dng_negative.h"
#include "dng_image_writer.h"
#include "dng_preview.h"
#include "dng_xmp_sdk.h"
#include "dng_memory_stream.h"
#include "dng_file_stream.h"
#include "dng_render.h"
#include "dng_image_writer.h"
#include "dng_color_space.h"
#include "dng_exceptions.h"
#include "dng_tag_values.h"
#include "dng_tag_codes.h"
#include "dng_xmp.h"
#include "dng_color_space.h"

#include "negativeProcessor.h"

#include "util.hpp"

RawConverter::RawConverter(const std::string& rawFilename) 
: m_appName("raw2dng")
, m_appVersion(RAW2DNG_VERSION_STR)
, m_dateTimeNow(std::make_unique<dng_date_time_info>())
{
    // -----------------------------------------------------------------------------------------
    // Init XMP SDK and some global variables we will need

    dng_xmp_sdk::InitializeSDK();
    CurrentDateTimeAndZone(*m_dateTimeNow);
    m_negProcessor = NegativeProcessor::createProcessor(rawFilename.c_str());
}

RawConverter::RawConverter(const std::string& rawFilename, const std::string& dcpFilename)
: RawConverter(rawFilename)
{

    m_negProcessor->setDNGPropertiesFromRaw();
    m_negProcessor->setCameraProfile(dcpFilename.c_str());

    dng_string appNameVersion(m_appName.c_str()); appNameVersion.Append(" "); appNameVersion.Append(m_appVersion.c_str());
    m_negProcessor->setExifFromRaw(*m_dateTimeNow, appNameVersion);
    m_negProcessor->setXmpFromRaw(*m_dateTimeNow, appNameVersion);

    m_negProcessor->rebuildIPTC(true);

    m_negProcessor->backupProprietaryData();

    // -----------------------------------------------------------------------------------------
    // Copy raw sensor data

    m_negProcessor->buildDNGImage();
}

RawConverter::~RawConverter() {
    dng_xmp_sdk::TerminateSDK();
    if (m_previewList) delete m_previewList;
}

void RawConverter::embedRaw(const std::string& rawFilename) {
    m_negProcessor->embedOriginalRaw(rawFilename.c_str());
}


void RawConverter::renderImage() {
    // -----------------------------------------------------------------------------------------
    // Render image

    try {
        m_negProcessor->renderImage();
    }
    catch (dng_exception& e) {
        std::stringstream error; error << "Error while rendering image from raw! (" << e.ErrorCode() << ": " << getDngErrorMessage(e.ErrorCode()) << ")";
        throw std::runtime_error(error.str());
    }
}


void RawConverter::renderPreviews() {
    // -----------------------------------------------------------------------------------------
    // Render JPEG and thumbnail previews

    m_previewList = new dng_preview_list();
    dng_render negRender(m_negProcessor->getDngRender());


    dng_jpeg_preview *jpeg_preview = new dng_jpeg_preview();
    jpeg_preview->fInfo.fApplicationName.Set_ASCII(m_appName.c_str());
    jpeg_preview->fInfo.fApplicationVersion.Set_ASCII(m_appVersion.c_str());
    jpeg_preview->fInfo.fDateTime = m_dateTimeNow->Encode_ISO_8601();
    jpeg_preview->fInfo.fColorSpace = previewColorSpace_sRGB;

    negRender.SetMaximumSize(1024);
    std::unique_ptr<dng_image> negImage(negRender.Render());
    dng_image_writer().EncodeJPEGPreview(m_negProcessor->getHost(), *negImage, *jpeg_preview, 5);
    AutoPtr<dng_preview> jp(jpeg_preview);
    m_previewList->Append(jp);

    dng_image_preview *thumbnail = new dng_image_preview();
    thumbnail->fInfo.fApplicationName    = jpeg_preview->fInfo.fApplicationName;
    thumbnail->fInfo.fApplicationVersion = jpeg_preview->fInfo.fApplicationVersion;
    thumbnail->fInfo.fDateTime           = jpeg_preview->fInfo.fDateTime;
    thumbnail->fInfo.fColorSpace         = jpeg_preview->fInfo.fColorSpace;

    negRender.SetMaximumSize(256);
    thumbnail->SetImage(m_negProcessor->getHost(), negRender.Render());
    AutoPtr<dng_preview> tn(thumbnail);
    m_previewList->Append(tn);
}


void RawConverter::writeDng(const std::string& outFilename) {
    // -----------------------------------------------------------------------------------------
    // Write DNG-image to file

    try {
        auto targetFile = dng_file_stream(outFilename.c_str(), true);
        dng_image_writer().WriteDNG(m_negProcessor->getHost(), targetFile, m_negProcessor->getNegative(), m_previewList);
    }
    catch (dng_exception& e) {
        std::stringstream error; error << "Error while writing DNG-file! (" << e.ErrorCode() << ": " << getDngErrorMessage(e.ErrorCode()) << ")";
        throw std::runtime_error(error.str());
    }
}


void RawConverter::writeTiff(const std::string& outFilename) {
    // -----------------------------------------------------------------------------------------
    // Render TIFF


    dng_render negRender(m_negProcessor->getDngRender());
    dng_image negImage(*negRender.Render());

    // -----------------------------------------------------------------------------------------
    // Write Tiff-image to file
    if (!m_previewList) renderPreviews();

    try {
        auto targetFile = dng_file_stream(outFilename.c_str(), true);
        dng_image_writer().WriteTIFF(m_negProcessor->getHost(), targetFile, negImage, piRGB, ccUncompressed,
                             &m_negProcessor->getDngMetadata(), &dng_space_sRGB::Get(), NULL,
                             dynamic_cast<const dng_jpeg_preview*>(&m_previewList->Preview(1)));
    }
    catch (dng_exception& e) {
        std::stringstream error; error << "Error while writing TIFF-file! (" << e.ErrorCode() << ": " << getDngErrorMessage(e.ErrorCode()) << ")";
        throw std::runtime_error(error.str());
    }
}


void RawConverter::writeJpeg(const std::string& outFilename) {
    // -----------------------------------------------------------------------------------------
    // Render JPEG

    // FIXME: we should render and integrate a thumbnail too


    dng_render negRender(m_negProcessor->getDngRender());
    dng_image negImage(*negRender.Render());

    dng_jpeg_preview jpeg;
    jpeg.fInfo.fApplicationName.Set_ASCII(m_appName.c_str());
    jpeg.fInfo.fApplicationVersion.Set_ASCII(m_appVersion.c_str());
    jpeg.fInfo.fDateTime = m_dateTimeNow->Encode_ISO_8601();
    jpeg.fInfo.fColorSpace = previewColorSpace_sRGB;

    dng_image_writer().EncodeJPEGPreview(m_negProcessor->getHost(), negImage, jpeg, 8);

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
        targetFile.Put((uint8*) jpeg.CompressedData().Buffer() + jfifHeaderLength, jpeg.CompressedData().LogicalSize() - jfifHeaderLength);

        targetFile.Flush();
    }
    catch (dng_exception& e) {
        std::stringstream error; error << "Error while writing JPEG-file! (" << e.ErrorCode() << ": " << getDngErrorMessage(e.ErrorCode()) << ")";
        throw std::runtime_error(error.str());
    }
}
