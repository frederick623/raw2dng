
#include "FujiProcessor.h"

#include <dng_simple_image.h>
#include <libraw/libraw.h>


// TODO/FIXME: Fuji support is currently broken!


FujiProcessor::FujiProcessor(dng_host&                    host,
                               std::unique_ptr<LibRaw>      rawProcessor,
                               Exiv2::Image::UniquePtr      rawImage)
    : BaseProcessor(host, std::move(rawProcessor), std::move(rawImage))
{
    m_fujiRotate90 = (2 == m_RawProcessor->COLOR(0, 1)) &&
                     (1 == m_RawProcessor->COLOR(1, 0));
}


void FujiProcessor::setDNGPropertiesFromRaw() {
    // Run the base implementation first, then apply Fuji-specific adjustments.
    BaseProcessor::setDNGPropertiesFromRaw();

    libraw_image_sizes_t* sizes   = &m_RawProcessor->imgdata.sizes;
    libraw_iparams_t*     iparams = &m_RawProcessor->imgdata.idata;

    // Orientation
    if (m_fujiRotate90)
        m_negative->SetBaseOrientation(
            m_negative->BaseOrientation() + dng_orientation::Mirror90CCW());

    // Mosaic
    if (iparams->colors != 4) m_negative->SetFujiMosaic(0);

    // Default scale and crop / active area (rotated variant)
    if (m_fujiRotate90) {
        m_negative->SetDefaultScale(
            dng_urational(sizes->iheight, sizes->height),
            dng_urational(sizes->iwidth,  sizes->width));
        m_negative->SetActiveArea(dng_rect(
            sizes->top_margin,  sizes->left_margin,
            sizes->top_margin  + sizes->width,
            sizes->left_margin + sizes->height));

        if (iparams->filters != 0) {
            m_negative->SetDefaultCropOrigin(8, 8);
            m_negative->SetDefaultCropSize(sizes->height - 16, sizes->width - 16);
        }
        else {
            m_negative->SetDefaultCropOrigin(0, 0);
            m_negative->SetDefaultCropSize(sizes->height, sizes->width);
        }
    }
}


void FujiProcessor::buildDNGImage() {
    BaseProcessor::buildDNGImage();

// TODO: FIXME – rotation of the dng_image bounds is unimplemented.
/*
    if (m_fujiRotate90) {
        dng_rect rotatedRect(dngImage->fBounds.W(), dngImage->fBounds.H());
        dngImage->fBounds = rotatedRect;
    }
*/
}