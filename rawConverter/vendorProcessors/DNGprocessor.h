
#pragma once

#include "BaseProcessor.h"


// ---------------------------------------------------------------------------
// DNGprocessor
//
// Handles source files that are already DNGs.  The constructor re-reads the
// file using the DNG SDK (which gives richer metadata than LibRaw/Exiv2).
// The pipeline steps are overridden to either no-op or minimally patch the
// already-correct values that the SDK parsed.
// ---------------------------------------------------------------------------
class DNGprocessor : public BaseProcessor<DNGprocessor> {
    // Allow produce() (a static member of the base) to call our constructor.
    friend class BaseProcessor<DNGprocessor>;

public:
    // Non-virtual pipeline-step overrides -----------------------------------

    // Only patches the filename; the DNG SDK already filled everything else.
    void setDNGPropertiesFromRaw();

    // Applies an external DCP if supplied; otherwise keeps the embedded one.
    void setCameraProfile(const char* dcpFilename);

    // Updates date + software tag; keeps all other Exif from the source DNG.
    void setExifFromRaw(const dng_date_time_info& dateTimeNow,
                        const dng_string&         appNameVersion);

    // Updates base XMP tags; keeps all other XMP from the source DNG.
    void setXmpFromRaw(const dng_date_time_info& dateTimeNow,
                       const dng_string&         appNameVersion);

    // No-op: proprietary data is already in the source DNG.
    void backupProprietaryData() {}

    // No-op: stage-1 image was already read by the DNG SDK in the constructor.
    void buildDNGImage() {}

protected:
    // Constructor parses the source DNG with the DNG SDK so that m_negative
    // is fully populated before any pipeline step runs.
    DNGprocessor(dng_host&                    host,
                 std::unique_ptr<LibRaw>      rawProcessor,
                 Exiv2::Image::UniquePtr      rawImage);
};