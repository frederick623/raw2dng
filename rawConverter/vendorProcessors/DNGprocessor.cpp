
#include "DNGprocessor.h"

#include <stdexcept>

#include <dng_camera_profile.h>
#include <dng_file_stream.h>
#include <dng_xmp.h>
#include <dng_info.h>


// ---------------------------------------------------------------------------
// Constructor
//
// Invoked by BaseProcessor<DNGprocessor>::produce() after the base-class
// BaseProcessor constructor has run (so m_negative already exists).
// Re-reads the source DNG with the DNG SDK so m_negative is fully populated
// before any pipeline step is called.
// ---------------------------------------------------------------------------
DNGprocessor::DNGprocessor(dng_host&                    host,
                            std::unique_ptr<LibRaw>      rawProcessor,
                            Exiv2::Image::UniquePtr      rawImage)
    : BaseProcessor(host, std::move(rawProcessor), std::move(rawImage))
{
    const std::string file(m_RawImage->io().path());

    try {
        dng_file_stream stream(file.c_str());

        dng_info info;
        info.Parse(m_host, stream);
        info.PostParse(m_host);
        if (!info.IsValidDNG()) throw dng_exception(dng_error_bad_format);

        m_negative->Parse(m_host, stream, info);
        m_negative->PostParse(m_host, stream, info);
        m_negative->ReadStage1Image(m_host, stream, info);
        m_negative->ReadTransparencyMask(m_host, stream, info);
        m_negative->ValidateRawImageDigest(m_host);
    }
    catch (const dng_exception&) { throw; }
    catch (...) { throw dng_exception(dng_error_unknown); }
}


// ---------------------------------------------------------------------------
// Pipeline step overrides
// ---------------------------------------------------------------------------

void DNGprocessor::setDNGPropertiesFromRaw() {
    // The DNG SDK already populated m_negative completely.
    // Only patch the original raw filename (cosmetic).
    std::string file(m_RawImage->io().path());
    size_t found = std::min(file.rfind("\\"), file.rfind("/"));
    if (found != std::string::npos)
        file = file.substr(found + 1, file.length() - found - 1);
    m_negative->SetOriginalRawFileName(file.c_str());
}

void DNGprocessor::setCameraProfile(const char* dcpFilename) {
    if (strlen(dcpFilename) == 0) return;  // keep what's already in the DNG

    AutoPtr<dng_camera_profile> prof(new dng_camera_profile);
    dng_file_stream profStream(dcpFilename);
    if (!prof->ParseExtended(profStream))
        throw std::runtime_error("Could not parse supplied camera profile file!");
    m_negative->AddProfile(prof);
}

void DNGprocessor::setExifFromRaw(const dng_date_time_info& dateTimeNow,
                                   const dng_string&         appNameVersion) {
    dng_exif* negExif = m_negative->GetExif();
    negExif->fDateTime = dateTimeNow;
    negExif->fSoftware = appNameVersion;
}

void DNGprocessor::setXmpFromRaw(const dng_date_time_info& dateTimeNow,
                                  const dng_string&         appNameVersion) {
    dng_xmp* negXmp = m_negative->GetXMP();
    negXmp->UpdateDateTime(dateTimeNow);
    negXmp->UpdateMetadataDate(dateTimeNow);
    negXmp->SetString(XMP_NS_XAP, "CreatorTool", appNameVersion);
    negXmp->Set(XMP_NS_DC, "format", "image/dng");
    negXmp->SetString(XMP_NS_PHOTOSHOP, "DateCreated",
                      m_negative->GetExif()->fDateTimeOriginal.Encode_ISO_8601());
}