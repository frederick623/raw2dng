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


#if defined(__ARM_NEON) || defined(__aarch64__)
#include <arm_neon.h>
#include "dng_filter_task.h"

inline float32x4_t fast_exp_neon(float32x4_t x) {
    // Computes e^-x for x >= 0 using 1 / (1 + x + 0.5x^2 + 0.1666x^3 + 0.0416x^4)
    float32x4_t one = vdupq_n_f32(1.0f);
    float32x4_t sum = vmlaq_f32(one, one, x);
    float32x4_t x2 = vmulq_f32(x, x);
    sum = vmlaq_f32(sum, vdupq_n_f32(0.5f), x2);
    float32x4_t x3 = vmulq_f32(x2, x);
    sum = vmlaq_f32(sum, vdupq_n_f32(0.16666667f), x3);
    float32x4_t x4 = vmulq_f32(x2, x2);
    sum = vmlaq_f32(sum, vdupq_n_f32(0.04166667f), x4);

    float32x4_t recip = vrecpeq_f32(sum);
    recip = vmulq_f32(vrecpsq_f32(sum, recip), recip);
    recip = vmulq_f32(vrecpsq_f32(sum, recip), recip);
    return recip;
}

class BilateralFilterNeonTask : public dng_filter_task {
private:
    float m_spatial_sigma;
    float m_range_sigma;
public:
    BilateralFilterNeonTask(const dng_image &src, dng_image &dst, float spatial_sigma, float range_sigma)
        : dng_filter_task("BilateralFilterNeonTask", src, dst),
          m_spatial_sigma(spatial_sigma), m_range_sigma(range_sigma) {}

    virtual dng_point UnitCell() const override {
        return dng_point(2, 2);
    }

    virtual dng_point MaxTileSize() const override {
        // A size of 256x256 means the tile will have 65536 pixels
        // Each pixel is a 16-bit int, so a tile is 128KB in size per plane.
        // This is extremely cache-friendly for CPU L2 cache.
        return dng_point(256, 256);
    }

    virtual dng_rect RepeatingTile1() const override {
        return dng_rect(0, 0, 256, 256); // Defines the base splitting interval
    }

    virtual dng_rect SrcArea(const dng_rect &dstArea) override {
        dng_rect srcArea = dstArea;
        srcArea.t = std::max(0, srcArea.t - 2);
        srcArea.b = std::min((int)fSrcImage.Bounds().b, srcArea.b + 2);
        srcArea.l = std::max(0, srcArea.l - 2);
        srcArea.r = std::min((int)fSrcImage.Bounds().r, srcArea.r + 2);
        return srcArea;
    }

    virtual dng_point SrcTileSize(const dng_point &dstTileSize) override {
        return dng_point(dstTileSize.v + 4, dstTileSize.h + 4);
    }

    virtual void ProcessArea(uint32 threadIndex, dng_pixel_buffer &srcBuffer, dng_pixel_buffer &dstBuffer) override {
        dng_rect area = dstBuffer.fArea;
        float w_o = std::exp(-4.0f / (2.0f * m_spatial_sigma * m_spatial_sigma));
        float w_d = std::exp(-8.0f / (2.0f * m_spatial_sigma * m_spatial_sigma));
        float range_denom = 1.0f / (2.0f * m_range_sigma * m_range_sigma);
        float32x4_t v_range_denom = vdupq_n_f32(range_denom);
        float32x4_t v_w_c = vdupq_n_f32(1.0f);
        float32x4_t v_w_o = vdupq_n_f32(w_o);
        float32x4_t v_w_d = vdupq_n_f32(w_d);

        for (int32 y = area.t; y < area.b; ++y) {
            if (y < 2 || y >= fSrcImage.Bounds().b - 2) {
                for (int32 x = area.l; x < area.r; ++x) {
                    dstBuffer.DirtyPixel_uint16(y, x, 0)[0] = srcBuffer.ConstPixel_uint16(y, x, 0)[0];
                }
                continue;
            }

            const uint16_t* r0 = srcBuffer.ConstPixel_uint16(y - 2, area.l, 0);
            const uint16_t* r1 = srcBuffer.ConstPixel_uint16(y, area.l, 0);
            const uint16_t* r2 = srcBuffer.ConstPixel_uint16(y + 2, area.l, 0);
            uint16_t* out_row = dstBuffer.DirtyPixel_uint16(y, area.l, 0);

            int32 x = area.l;
            for (; x < area.r - 15; x += 16) {
                if (x < 2 || x >= fSrcImage.Bounds().r - 18) {
                    for (int i=0; i<16; ++i) out_row[x - area.l + i] = r1[x - area.l + i];
                    continue;
                }

                int off = x - area.l;
                uint16x8x2_t val1_center = vld2q_u16(r1 + off);
                uint16x8x2_t val1_left   = vld2q_u16(r1 + off - 2);
                uint16x8x2_t val1_right  = vld2q_u16(r1 + off + 2);

                uint16x8x2_t val0_center = vld2q_u16(r0 + off);
                uint16x8x2_t val0_left   = vld2q_u16(r0 + off - 2);
                uint16x8x2_t val0_right  = vld2q_u16(r0 + off + 2);

                uint16x8x2_t val2_center = vld2q_u16(r2 + off);
                uint16x8x2_t val2_left   = vld2q_u16(r2 + off - 2);
                uint16x8x2_t val2_right  = vld2q_u16(r2 + off + 2);

                uint16x8x2_t result;

                for (int c = 0; c < 2; ++c) {
                    float32x4_t f_c_low = vcvtq_f32_u32(vmovl_u16(vget_low_u16(val1_center.val[c])));
                    float32x4_t f_c_high = vcvtq_f32_u32(vmovl_u16(vget_high_u16(val1_center.val[c])));

                    float32x4_t sum_w_low = v_w_c, sum_w_high = v_w_c;
                    float32x4_t sum_v_low = f_c_low, sum_v_high = f_c_high;

                    auto process = [&](uint16x8_t neighbor, float32x4_t spatial_w) {
                        float32x4_t f_n_low = vcvtq_f32_u32(vmovl_u16(vget_low_u16(neighbor)));
                        float32x4_t f_n_high = vcvtq_f32_u32(vmovl_u16(vget_high_u16(neighbor)));
                        float32x4_t diff_low = vsubq_f32(f_n_low, f_c_low);
                        float32x4_t diff_high = vsubq_f32(f_n_high, f_c_high);
                        float32x4_t w_low = vmulq_f32(spatial_w, fast_exp_neon(vmulq_f32(vmulq_f32(diff_low, diff_low), v_range_denom)));
                        float32x4_t w_high = vmulq_f32(spatial_w, fast_exp_neon(vmulq_f32(vmulq_f32(diff_high, diff_high), v_range_denom)));
                        sum_w_low = vaddq_f32(sum_w_low, w_low); sum_w_high = vaddq_f32(sum_w_high, w_high);
                        sum_v_low = vmlaq_f32(sum_v_low, w_low, f_n_low); sum_v_high = vmlaq_f32(sum_v_high, w_high, f_n_high);
                    };

                    process(val1_left.val[c], v_w_o); process(val1_right.val[c], v_w_o);
                    process(val0_center.val[c], v_w_o); process(val2_center.val[c], v_w_o);
                    process(val0_left.val[c], v_w_d); process(val0_right.val[c], v_w_d);
                    process(val2_left.val[c], v_w_d); process(val2_right.val[c], v_w_d);

                    float32x4_t r_low = vmulq_f32(sum_v_low, vrecpeq_f32(sum_w_low));
                    float32x4_t r_high = vmulq_f32(sum_v_high, vrecpeq_f32(sum_w_high));

                    result.val[c] = vcombine_u16(vqmovn_u32(vcvtq_u32_f32(r_low)), vqmovn_u32(vcvtq_u32_f32(r_high)));
                }

                vst2q_u16(out_row + off, result);
            }

            for (; x < area.r; ++x) out_row[x - area.l] = r1[x - area.l];
        }
    }
};
#endif

#include <dng_simple_image.h>

void NegativeProcessor::renderImage() {
    m_negative->BuildStage2Image(m_host);

#if defined(__ARM_NEON) || defined(__aarch64__)
    const dng_image *stage2 = m_negative->Stage2Image();
    if (stage2 && stage2->PixelType() == ttShort && stage2->Planes() == 1) { // Process Bayer
        AutoPtr<dng_image> filtered(new dng_simple_image(stage2->Bounds(), stage2->Planes(), stage2->PixelType(), m_host.Allocator()));
        BilateralFilterNeonTask filterTask(*stage2, *filtered, 2.0f, 1000.0f); // Adjust sigmas as needed
        m_host.PerformAreaTask(filterTask, stage2->Bounds());
        m_negative->SetStage2Image(filtered);
    }
#endif

    m_negative->BuildStage3Image(m_host);
}
