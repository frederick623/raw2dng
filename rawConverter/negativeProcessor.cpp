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

   This file uses code from dngconvert from Jens Mueller and others
   (https://github.com/jmue/dngconvert) - Copyright (C) 2011 Jens 
   Mueller (tschensinger at gmx dot de)
*/

#include "config.h"
#include "negativeProcessor.h"
#include "vendorProcessors/DNGprocessor.h"
#include "vendorProcessors/ILCE7processor.h"
#include "vendorProcessors/FujiProcessor.h"
#include "vendorProcessors/variousVendorProcessor.h"

#include <stdexcept>
#include <iostream>

#include <dng_date_time.h>
#include <dng_image_writer.h>

const char* getDngErrorMessage(int errorCode) {
    switch (errorCode) {
        default:
        case 100000: return "Unknown error";
        case 100003: return "Processing stopped by user (or host application) request";
        case 100004: return "Necessary host functionality is not present";
        case 100005: return "Out of memory";
        case 100006: return "File format is not valid";
        case 100007: return "Matrix has wrong shape, is badly conditioned, or similar problem";
        case 100008: return "Could not open file";
        case 100009: return "Error reading file";
        case 100010: return "Error writing file";
        case 100011: return "Unexpected end of file";
        case 100012: return "File is damaged in some way";
        case 100013: return "Image is too big to save as DNG";
        case 100014: return "Image is too big to save as TIFF";
        case 100015: return "DNG version is unsupported";
    }
}


NegativeProcessor::~NegativeProcessor()
{
    if (m_previewList) delete m_previewList;
}

bool NegativeProcessor::operator==(const NegativeProcessor& candidate) const
{
    const dng_image *refImage = m_negative->Stage1Image();
    const dng_image *candImage = candidate.m_negative->Stage1Image();

    if (refImage->Bounds() != candImage->Bounds()) {
        return false;
    }
    if (refImage->Planes() != candImage->Planes()) {
        return false;
    }
    if (refImage->PixelType() != candImage->PixelType()) {
        return false;
    }
    if (m_negative->ColorChannels() != candidate.m_negative->ColorChannels()) {
        return false;
    }

    for (uint32 plane = 0; plane < refImage->Planes(); ++plane) {
        if (m_negative->WhiteLevel(plane) != candidate.m_negative->WhiteLevel(plane)) {
            return false;
        }
    }

    const dng_mosaic_info *refMosaic = m_negative->GetMosaicInfo();
    const dng_mosaic_info *candMosaic = candidate.m_negative->GetMosaicInfo();
    if ((refMosaic == nullptr) != (candMosaic == nullptr)) {
        return false;
    }
    if (refMosaic && candMosaic) {
        if (refMosaic->fCFAPatternSize != candMosaic->fCFAPatternSize ||
            refMosaic->fColorPlanes != candMosaic->fColorPlanes ||
            refMosaic->fCFALayout != candMosaic->fCFALayout ||
            std::memcmp(refMosaic->fCFAPattern, candMosaic->fCFAPattern, sizeof(refMosaic->fCFAPattern)) != 0 ||
            std::memcmp(refMosaic->fCFAPlaneColor, candMosaic->fCFAPlaneColor, sizeof(refMosaic->fCFAPlaneColor)) != 0) {
            return false;
        }
    }
    return true;
}

std::unique_ptr<NegativeProcessor>  NegativeProcessor::createProcessor(dng_host& host, const char *filename, const char* dcpFilename) {
    // -----------------------------------------------------------------------------------------
    // Open and parse rawfile with libraw...
    auto rawProcessor = getLibRaw((filename));
    // -----------------------------------------------------------------------------------------
    // ...and libexiv2
    auto rawImage = getExivImage(filename);
    // -----------------------------------------------------------------------------------------
    // Identify and create correct processor class
    std::unique_ptr<dng_negative> negative;
    if (rawProcessor->imgdata.idata.dng_version != 0) {
        try {
            negative = DNGprocessor::produce(host, std::move(rawProcessor), std::move(rawImage), dcpFilename);
        }
        catch (dng_exception &e) {
            std::stringstream error; error << "Cannot parse source DNG-file (" << e.ErrorCode() << ": " << getDngErrorMessage(e.ErrorCode()) << ")";
            throw std::runtime_error(error.str());
        }
    }
    else if (!strncmp(rawProcessor->imgdata.idata.model, "ILCE-7", 6))
        negative = ILCE7processor::produce(host, std::move(rawProcessor), std::move(rawImage), dcpFilename);
    else if (!strcmp(rawProcessor->imgdata.idata.make, "FUJIFILM"))
        negative = FujiProcessor::produce(host, std::move(rawProcessor), std::move(rawImage), dcpFilename);
    else
        negative = VariousVendorProcessor::produce(host, std::move(rawProcessor), std::move(rawImage), dcpFilename);

    return std::make_unique<NegativeProcessor>(host, std::move(negative));
}


NegativeProcessor::NegativeProcessor(dng_host& host, std::unique_ptr<dng_negative> negative)
                                    : m_host(host)
                                    , m_negative(std::move(negative))
{

}


ColorKeyCode colorKey(const char color) {
    switch (color) {
        case 'R': return colorKeyRed;
        case 'G': return colorKeyGreen;
        case 'B': return colorKeyBlue;
        case 'C': return colorKeyCyan;
        case 'M': return colorKeyMagenta;
        case 'Y': return colorKeyYellow;
        case 'E': return colorKeyCyan; // only in the Sony DSC-F828. 'Emerald' - like cyan according to Sony
        case 'T':                      // what is 'T'??? LibRaw reports that only for the Leaf Catchlight, so I guess we're not compatible with early '90s tech...
        default:  return colorKeyMaxEnum;
    }
}


std::shared_ptr<dng_jpeg_preview> NegativeProcessor::getJpegPreview() {
    auto jpeg_preview = std::make_shared<dng_jpeg_preview>();
    jpeg_preview->fInfo.fApplicationName.Set_ASCII(m_appName.c_str());
    jpeg_preview->fInfo.fApplicationVersion.Set_ASCII(m_appVersion.c_str());
    jpeg_preview->fInfo.fDateTime = m_dateTimeNow.Encode_ISO_8601();
    jpeg_preview->fInfo.fColorSpace = previewColorSpace_sRGB;
    return jpeg_preview;
}


void NegativeProcessor::renderPreviews() {
    // -----------------------------------------------------------------------------------------
    // Render JPEG and thumbnail previews

    m_previewList = new dng_preview_list();
    dng_render negRender(getDngRender());

    auto jpeg_preview = getJpegPreview();
    negRender.SetMaximumSize(1024);
    std::unique_ptr<dng_image> negImage(negRender.Render());
    dng_image_writer().EncodeJPEGPreview(m_host, *negImage, *jpeg_preview, 5);
    m_previewList->Append(jpeg_preview);

    auto thumbnail = getJpegPreview();
    negRender.SetMaximumSize(256);
    thumbnail->SetImage(m_host, negRender.Render());
    m_previewList->Append(std::shared_ptr<dng_preview>(thumbnail));
}
