
#pragma once

#include <memory>
#include <string>
#include <sstream>
#include <stdexcept>
#include <algorithm>
#include <iostream>

#include "negativeProcessor.h"

#include <dng_date_time.h>
#include <dng_simple_image.h>
#include <dng_camera_profile.h>
#include <dng_file_stream.h>
#include <dng_memory_stream.h>
#include <dng_xmp.h>
#include <dng_image_writer.h>
#include <dng_negative.h>
#include <dng_exif.h>
#include <dng_host.h>

#include <exiv2/image.hpp>
#include <exiv2/xmp_exiv2.hpp>
#include <libraw/libraw.h>

#include <zlib.h>

// ---------------------------------------------------------------------------
// Internal helper (file-local)
// ---------------------------------------------------------------------------

namespace detail {

inline ColorKeyCode colorKey(const char color) {
    switch (color) {
        case 'R': return colorKeyRed;
        case 'G': return colorKeyGreen;
        case 'B': return colorKeyBlue;
        case 'C': return colorKeyCyan;
        case 'M': return colorKeyMagenta;
        case 'Y': return colorKeyYellow;
        case 'E': return colorKeyCyan;  // Sony DSC-F828 "Emerald" ≈ Cyan
        case 'T':                       // Leaf Catchlight – unsupported
        default:  return colorKeyMaxEnum;
    }
}

} // namespace detail


// ===========================================================================
// BaseProcessor<Derived>
//
// CRTP base class that owns all LibRaw + Exiv2 state and implements the full
// RAW → dng_negative conversion pipeline.
//
// Usage pattern
// -------------
//   class MyProcessor : public BaseProcessor<MyProcessor> {
//       friend class BaseProcessor<MyProcessor>;   // for produce()
//   protected:
//       MyProcessor(dng_host&, std::unique_ptr<LibRaw>, Exiv2::Image::UniquePtr);
//   public:
//       // Override any subset of the pipeline steps (non-virtual):
//       void setDNGPropertiesFromRaw();
//       void setExifFromRaw(const dng_date_time_info&, const dng_string&);
//       ...
//   };
//
//   // Build a finished NegativeProcessor:
//   auto neg = MyProcessor::produce(host, rawProcessor, rawImage, dcpFilename);
//
// Why CRTP instead of virtual?
// ----------------------------
// The classic antipattern here is calling virtual functions from the base
// constructor – at that point the derived vtable is not yet installed, so the
// base-class versions always run and overrides are silently ignored.
//
// With CRTP the concrete type is known at compile time.  produce() constructs
// the Derived object (all constructors are done) and then calls each pipeline
// step as a plain member-function call on a Derived&, giving correct static
// dispatch with zero vtable overhead.
// ===========================================================================

template<typename Derived>
class BaseProcessor {
public:

    static void embedOriginalRaw(dng_host& host, dng_negative& negative, const char* rawFilename) {
        constexpr uint32 BLOCKSIZE = 65536;   // as per DNG spec

        dng_file_stream rawDataStream(rawFilename);
        rawDataStream.SetReadPosition(0);

        uint32 rawFileSize    = static_cast<uint32>(rawDataStream.Length());
        uint32 numberRawBlocks = static_cast<uint32>(
            std::floor((rawFileSize + (BLOCKSIZE - 1)) / BLOCKSIZE));

        // Write header with placeholder block offsets.
        dng_memory_stream embeddedRawStream(host.Allocator());
        embeddedRawStream.SetBigEndian(true);
        embeddedRawStream.Put_uint32(rawFileSize);
        for (uint32 block = 0; block < numberRawBlocks; block++)
            embeddedRawStream.Put_uint32(0);   // placeholder offset
        embeddedRawStream.Put_uint32(0);       // index to next data fork

        uint32 indexOffset = 1 * sizeof(uint32);
        uint32 dataOffset  = (numberRawBlocks + 1 + 1) * sizeof(uint32);

        for (uint32 block = 0; block < numberRawBlocks; block++) {
            z_stream zstrm;
            zstrm.zalloc = Z_NULL;
            zstrm.zfree  = Z_NULL;
            zstrm.opaque = Z_NULL;
            if (deflateInit(&zstrm, Z_DEFAULT_COMPRESSION) != Z_OK)
                throw std::runtime_error("Error initialising ZLib for embedding raw file!");

            unsigned char inBuffer[BLOCKSIZE], outBuffer[BLOCKSIZE * 2];
            uint32 currentRawBlockLength = static_cast<uint32>(
                std::min(static_cast<uint64>(BLOCKSIZE),
                         rawFileSize - rawDataStream.Position()));
            rawDataStream.Get(inBuffer, currentRawBlockLength);
            zstrm.avail_in  = currentRawBlockLength;
            zstrm.next_in   = inBuffer;
            zstrm.avail_out = BLOCKSIZE * 2;
            zstrm.next_out  = outBuffer;
            if (deflate(&zstrm, Z_FINISH) != Z_STREAM_END)
                throw std::runtime_error("Error compressing chunk for embedding raw file!");

            uint32 compressedBlockLength = zstrm.total_out;
            deflateEnd(&zstrm);

            embeddedRawStream.SetWritePosition(indexOffset);
            embeddedRawStream.Put_uint32(dataOffset);
            indexOffset += sizeof(uint32);

            embeddedRawStream.SetWritePosition(dataOffset);
            embeddedRawStream.Put(outBuffer, compressedBlockLength);
            dataOffset += compressedBlockLength;
        }

        embeddedRawStream.SetWritePosition(indexOffset);
        embeddedRawStream.Put_uint32(dataOffset);

        // Seven empty "Mac OS forks" as per the DNG spec.
        embeddedRawStream.SetWritePosition(dataOffset);
        for (int i = 0; i < 7; i++)
            embeddedRawStream.Put_uint32(0);

        AutoPtr<dng_memory_block> block(
            embeddedRawStream.AsMemoryBlock(host.Allocator()));
        negative.SetOriginalRawFileData(block);
        negative.FindOriginalRawFileDigest();
    }

    // -----------------------------------------------------------------------
    // produce() – the single public entry point.
    //
    // Creates a Derived instance (all constructors complete), runs every
    // pipeline step via static dispatch on the concrete type, then transfers
    // ownership of the finished dng_negative into a NegativeProcessor.
    // -----------------------------------------------------------------------
    static std::unique_ptr<dng_negative> produce(
            dng_host&                    host,
            std::unique_ptr<LibRaw>      rawProcessor,
            Exiv2::Image::UniquePtr      rawImage,
            const char*                  dcpFilename = "",
            bool                         embedRaw = false) {

        dng_date_time_info dateTimeNow;
        CurrentDateTimeAndZone(dateTimeNow);

        const std::string appName    = "raw2dng";
        const std::string appVersion = RAW2DNG_VERSION_STR;
        dng_string appNameVersion((appName + appVersion).c_str());

        // ------------------------------------------------------------------
        // 1. Construct the concrete processor.
        //    All constructors run before any pipeline step is invoked,
        //    so derived constructors (e.g. DNGprocessor re-parsing the DNG)
        //    complete first.
        // ------------------------------------------------------------------
        Derived proc(host, std::move(rawProcessor), std::move(rawImage));

        // ------------------------------------------------------------------
        // 2. Pipeline – each call resolves at compile time to the most
        //    derived override that exists, with no virtual dispatch.
        // ------------------------------------------------------------------
        proc.setDNGPropertiesFromRaw();
        proc.setCameraProfile(dcpFilename);
        proc.setExifFromRaw(dateTimeNow, appNameVersion);
        proc.setXmpFromRaw(dateTimeNow, appNameVersion);
        proc.m_negative->RebuildIPTC(true);
        proc.backupProprietaryData();
        proc.buildDNGImage();

        // ------------------------------------------------------------------
        // 3. Transfer the finished negative into a NegativeProcessor.
        // ------------------------------------------------------------------
        return std::move(proc.m_negative);
    }


    // -----------------------------------------------------------------------
    // Default pipeline step implementations.
    // Derived classes shadow (not override) whichever steps they customise.
    // -----------------------------------------------------------------------

    void setDNGPropertiesFromRaw() {
        libraw_image_sizes_t* sizes   = &m_RawProcessor->imgdata.sizes;
        libraw_iparams_t*     iparams = &m_RawProcessor->imgdata.idata;

        // Raw filename
        std::string file(m_RawImage->io().path());
        size_t found = std::min(file.rfind("\\"), file.rfind("/"));
        if (found != std::string::npos)
            file = file.substr(found + 1, file.length() - found - 1);
        m_negative->SetOriginalRawFileName(file.c_str());

        // Model
        dng_string makeModel;
        makeModel.Append(iparams->make);
        makeModel.Append(" ");
        makeModel.Append(iparams->model);
        m_negative->SetModelName(makeModel.Get());

        // Orientation
        switch (sizes->flip) {
            case 180:
            case 3:  m_negative->SetBaseOrientation(dng_orientation::Rotate180());   break;
            case 270:
            case 5:  m_negative->SetBaseOrientation(dng_orientation::Rotate90CCW()); break;
            case 90:
            case 6:  m_negative->SetBaseOrientation(dng_orientation::Rotate90CW());  break;
            default: m_negative->SetBaseOrientation(dng_orientation::Normal());       break;
        }

        // ColorKeys (must precede Mosaic)
        m_negative->SetColorChannels(iparams->colors);
        m_negative->SetColorKeys(detail::colorKey(iparams->cdesc[0]),
                                 detail::colorKey(iparams->cdesc[1]),
                                 detail::colorKey(iparams->cdesc[2]),
                                 detail::colorKey(iparams->cdesc[3]));

        // Mosaic
        if (iparams->colors == 4) m_negative->SetQuadMosaic(iparams->filters);
        else switch (iparams->filters) {
            case 0xe1e1e1e1: m_negative->SetBayerMosaic(0); break;
            case 0xb4b4b4b4: m_negative->SetBayerMosaic(1); break;
            case 0x1e1e1e1e: m_negative->SetBayerMosaic(2); break;
            case 0x4b4b4b4b: m_negative->SetBayerMosaic(3); break;
            default: break;   // subclass may handle this (e.g. Fuji)
        }

        // Default scale + crop / active area
        m_negative->SetDefaultScale(
            dng_urational(sizes->iwidth,  sizes->width),
            dng_urational(sizes->iheight, sizes->height));
        m_negative->SetActiveArea(dng_rect(
            sizes->top_margin,  sizes->left_margin,
            sizes->top_margin  + sizes->height,
            sizes->left_margin + sizes->width));

        uint32 cropWidth, cropHeight;
        if (!getRawExifTag("Exif.Photo.PixelXDimension", 0, &cropWidth) ||
            !getRawExifTag("Exif.Photo.PixelYDimension", 0, &cropHeight)) {
            cropWidth  = sizes->width  - 16;
            cropHeight = sizes->height - 16;
        }
        int cropLeftMargin = (cropWidth  > sizes->width)  ? 0 : (sizes->width  - cropWidth)  / 2;
        int cropTopMargin  = (cropHeight > sizes->height) ? 0 : (sizes->height - cropHeight) / 2;
        m_negative->SetDefaultCropOrigin(cropLeftMargin, cropTopMargin);
        m_negative->SetDefaultCropSize(cropWidth, cropHeight);

        // CameraNeutral
        dng_vector cameraNeutral(iparams->colors);
        for (int i = 0; i < iparams->colors; i++)
            cameraNeutral[i] = (m_RawProcessor->imgdata.color.cam_mul[i] == 0)
                               ? 0.0
                               : 1.0 / m_RawProcessor->imgdata.color.cam_mul[i];
        m_negative->SetCameraNeutral(cameraNeutral);

        // Black / White levels
        libraw_colordata_t* colors = &m_RawProcessor->imgdata.color;
        for (int i = 0; i < 4; i++)
            m_negative->SetWhiteLevel(static_cast<uint32>(colors->maximum), i);

        if (m_negative->GetMosaicInfo() != nullptr &&
            m_negative->GetMosaicInfo()->fCFAPatternSize == dng_point(2, 2))
            m_negative->SetQuadBlacks(colors->black + colors->cblack[0],
                                      colors->black + colors->cblack[1],
                                      colors->black + colors->cblack[2],
                                      colors->black + colors->cblack[3]);
        else
            m_negative->SetBlackLevel(colors->black + colors->cblack[0], 0);

        // Fixed properties (defaults)
        m_negative->SetBaselineExposure(0.0);
        m_negative->SetBaselineNoise(1.0);
        m_negative->SetBaselineSharpness(1.0);
        m_negative->SetAntiAliasStrength(dng_urational(100, 100));
        m_negative->SetLinearResponseLimit(1.0);
        m_negative->SetAnalogBalance(dng_vector_3(1.0, 1.0, 1.0));
        m_negative->SetShadowScale(dng_urational(1, 1));
    }


    void setCameraProfile(const char* dcpFilename) {
        AutoPtr<dng_camera_profile> prof(new dng_camera_profile);

        if (strlen(dcpFilename) > 0) {
            dng_file_stream profStream(dcpFilename);
            if (!prof->ParseExtended(profStream))
                throw std::runtime_error("Could not parse supplied camera profile file!");
        }
        else {
            // Build a minimal profile from LibRaw's colour matrix.
            dng_string profName;
            profName.Append(m_RawProcessor->imgdata.idata.make);
            profName.Append(" ");
            profName.Append(m_RawProcessor->imgdata.idata.model);

            prof->SetName(profName.Get());
            prof->SetCalibrationIlluminant1(lsD65);

            int colors = m_RawProcessor->imgdata.idata.colors;
            if ((colors == 3) || (colors == 4)) {
                dng_matrix* colormatrix1 = new dng_matrix(colors, 3);
                for (int i = 0; i < colors; i++)
                    for (int j = 0; j < 3; j++)
                        (*colormatrix1)[i][j] = m_RawProcessor->imgdata.color.cam_xyz[i][j];

                if (colormatrix1->MaxEntry() == 0.0) {
                    printf("Warning, camera XYZ Matrix is null\n");
                    delete colormatrix1;
                    if (colors == 3)
                        colormatrix1 = new dng_matrix_3by3(
                            1.0, 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 1.0);
                    else
                        colormatrix1 = new dng_matrix_4by3(
                            0.0, 0.0, 0.0, 1.0, 0.0, 0.0,
                            0.0, 1.0, 0.0, 0.0, 0.0, 1.0);
                }
                prof->SetColorMatrix1(*colormatrix1);
                delete colormatrix1;
            }
            prof->SetProfileCalibrationSignature("com.fimagena.raw2dng");
        }

        m_negative->AddProfile(prof);
    }



    void setExifFromRaw(const dng_date_time_info& dateTimeNow,
                        const dng_string&         appNameVersion) {
        dng_exif* negExif = m_negative->GetExif();

        // TIFF 6.0 "D. Other Tags"
        getRawExifTag("Exif.Image.DateTime",         &negExif->fDateTime);
        getRawExifTag("Exif.Image.ImageDescription", &negExif->fImageDescription);
        getRawExifTag("Exif.Image.Make",             &negExif->fMake);
        getRawExifTag("Exif.Image.Model",            &negExif->fModel);
        getRawExifTag("Exif.Image.Software",         &negExif->fSoftware);
        getRawExifTag("Exif.Image.Artist",           &negExif->fArtist);
        getRawExifTag("Exif.Image.Copyright",        &negExif->fCopyright);

        // Exif 2.3 "A. Tags Relating to Version"
        getRawExifTag("Exif.Photo.ExifVersion", 0, &negExif->fExifVersion);

        // Exif 2.3 "B. Tags Relating to Image data Characteristics"
        getRawExifTag("Exif.Photo.ColorSpace", 0, &negExif->fColorSpace);

        // Exif 2.3 "C. Tags Relating To Image Configuration"
        getRawExifTag("Exif.Photo.ComponentsConfiguration",  0, &negExif->fComponentsConfiguration);
        getRawExifTag("Exif.Photo.CompressedBitsPerPixel",   0, &negExif->fCompresssedBitsPerPixel);
        getRawExifTag("Exif.Photo.PixelXDimension",          0, &negExif->fPixelXDimension);
        getRawExifTag("Exif.Photo.PixelYDimension",          0, &negExif->fPixelYDimension);

        // Exif 2.3 "D. Tags Relating to User Information"
        getRawExifTag("Exif.Photo.UserComment", &negExif->fUserComment);

        // Exif 2.3 "F. Tags Relating to Date and Time"
        getRawExifTag("Exif.Photo.DateTimeOriginal",  &negExif->fDateTimeOriginal);
        getRawExifTag("Exif.Photo.DateTimeDigitized", &negExif->fDateTimeDigitized);

        // Exif 2.3 "G. Tags Relating to Picture-Taking Conditions"
        getRawExifTag("Exif.Photo.ExposureTime",              0, &negExif->fExposureTime);
        getRawExifTag("Exif.Photo.FNumber",                   0, &negExif->fFNumber);
        getRawExifTag("Exif.Photo.ExposureProgram",           0, &negExif->fExposureProgram);
        getRawExifTag("Exif.Photo.ISOSpeedRatings",           negExif->fISOSpeedRatings, 3);
        getRawExifTag("Exif.Photo.SensitivityType",           0, &negExif->fSensitivityType);
        getRawExifTag("Exif.Photo.StandardOutputSensitivity", 0, &negExif->fStandardOutputSensitivity);
        getRawExifTag("Exif.Photo.RecommendedExposureIndex",  0, &negExif->fRecommendedExposureIndex);
        getRawExifTag("Exif.Photo.ISOSpeed",                  0, &negExif->fISOSpeed);
        getRawExifTag("Exif.Photo.ISOSpeedLatitudeyyy",       0, &negExif->fISOSpeedLatitudeyyy);
        getRawExifTag("Exif.Photo.ISOSpeedLatitudezzz",       0, &negExif->fISOSpeedLatitudezzz);
        getRawExifTag("Exif.Photo.ShutterSpeedValue",         0, &negExif->fShutterSpeedValue);
        getRawExifTag("Exif.Photo.ApertureValue",             0, &negExif->fApertureValue);
        getRawExifTag("Exif.Photo.BrightnessValue",           0, &negExif->fBrightnessValue);
        getRawExifTag("Exif.Photo.ExposureBiasValue",         0, &negExif->fExposureBiasValue);
        getRawExifTag("Exif.Photo.MaxApertureValue",          0, &negExif->fMaxApertureValue);
        getRawExifTag("Exif.Photo.SubjectDistance",           0, &negExif->fSubjectDistance);
        getRawExifTag("Exif.Photo.MeteringMode",              0, &negExif->fMeteringMode);
        getRawExifTag("Exif.Photo.LightSource",               0, &negExif->fLightSource);
        getRawExifTag("Exif.Photo.Flash",                     0, &negExif->fFlash);
        getRawExifTag("Exif.Photo.FocalLength",               0, &negExif->fFocalLength);
        negExif->fSubjectAreaCount =
            getRawExifTag("Exif.Photo.SubjectArea", negExif->fSubjectArea, 4);
        getRawExifTag("Exif.Photo.FocalPlaneXResolution",    0, &negExif->fFocalPlaneXResolution);
        getRawExifTag("Exif.Photo.FocalPlaneYResolution",    0, &negExif->fFocalPlaneYResolution);
        getRawExifTag("Exif.Photo.FocalPlaneResolutionUnit", 0, &negExif->fFocalPlaneResolutionUnit);
        getRawExifTag("Exif.Photo.ExposureIndex",            0, &negExif->fExposureIndex);
        getRawExifTag("Exif.Photo.SensingMethod",            0, &negExif->fSensingMethod);
        getRawExifTag("Exif.Photo.FileSource",               0, &negExif->fFileSource);
        getRawExifTag("Exif.Photo.SceneType",                0, &negExif->fSceneType);
        getRawExifTag("Exif.Photo.CustomRendered",           0, &negExif->fCustomRendered);
        getRawExifTag("Exif.Photo.ExposureMode",             0, &negExif->fExposureMode);
        getRawExifTag("Exif.Photo.WhiteBalance",             0, &negExif->fWhiteBalance);
        getRawExifTag("Exif.Photo.DigitalZoomRatio",         0, &negExif->fDigitalZoomRatio);
        getRawExifTag("Exif.Photo.FocalLengthIn35mmFilm",   0, &negExif->fFocalLengthIn35mmFilm);
        getRawExifTag("Exif.Photo.SceneCaptureType",         0, &negExif->fSceneCaptureType);
        getRawExifTag("Exif.Photo.GainControl",              0, &negExif->fGainControl);
        getRawExifTag("Exif.Photo.Contrast",                 0, &negExif->fContrast);
        getRawExifTag("Exif.Photo.Saturation",               0, &negExif->fSaturation);
        getRawExifTag("Exif.Photo.Sharpness",                0, &negExif->fSharpness);
        getRawExifTag("Exif.Photo.SubjectDistanceRange",     0, &negExif->fSubjectDistanceRange);

        // Exif 2.3 "H. Other Tags"
        getRawExifTag("Exif.Photo.CameraOwnerName",    &negExif->fOwnerName);
        getRawExifTag("Exif.Photo.BodySerialNumber",   &negExif->fCameraSerialNumber);
        getRawExifTag("Exif.Photo.LensSpecification",  negExif->fLensInfo, 4);
        getRawExifTag("Exif.Photo.LensMake",           &negExif->fLensMake);
        getRawExifTag("Exif.Photo.LensModel",          &negExif->fLensName);
        getRawExifTag("Exif.Photo.LensSerialNumber",   &negExif->fLensSerialNumber);

        // GPS
        uint32 gpsVer[4]; gpsVer[0] = gpsVer[1] = gpsVer[2] = gpsVer[3] = 0;
        getRawExifTag("Exif.GPSInfo.GPSVersionID", gpsVer, 4);
        negExif->fGPSVersionID = (gpsVer[0] << 24) + (gpsVer[1] << 16) +
                                 (gpsVer[2] <<  8) +  gpsVer[3];
        getRawExifTag("Exif.GPSInfo.GPSLatitudeRef",      &negExif->fGPSLatitudeRef);
        getRawExifTag("Exif.GPSInfo.GPSLatitude",          negExif->fGPSLatitude,  3);
        getRawExifTag("Exif.GPSInfo.GPSLongitudeRef",     &negExif->fGPSLongitudeRef);
        getRawExifTag("Exif.GPSInfo.GPSLongitude",         negExif->fGPSLongitude, 3);
        getRawExifTag("Exif.GPSInfo.GPSAltitudeRef",   0, &negExif->fGPSAltitudeRef);
        getRawExifTag("Exif.GPSInfo.GPSAltitude",      0, &negExif->fGPSAltitude);
        getRawExifTag("Exif.GPSInfo.GPSTimeStamp",         negExif->fGPSTimeStamp, 3);
        getRawExifTag("Exif.GPSInfo.GPSSatellites",       &negExif->fGPSSatellites);
        getRawExifTag("Exif.GPSInfo.GPSStatus",           &negExif->fGPSStatus);
        getRawExifTag("Exif.GPSInfo.GPSMeasureMode",      &negExif->fGPSMeasureMode);
        getRawExifTag("Exif.GPSInfo.GPSDOP",           0, &negExif->fGPSDOP);
        getRawExifTag("Exif.GPSInfo.GPSSpeedRef",         &negExif->fGPSSpeedRef);
        getRawExifTag("Exif.GPSInfo.GPSSpeed",         0, &negExif->fGPSSpeed);
        getRawExifTag("Exif.GPSInfo.GPSTrackRef",         &negExif->fGPSTrackRef);
        getRawExifTag("Exif.GPSInfo.GPSTrack",         0, &negExif->fGPSTrack);
        getRawExifTag("Exif.GPSInfo.GPSImgDirectionRef",  &negExif->fGPSImgDirectionRef);
        getRawExifTag("Exif.GPSInfo.GPSImgDirection",  0, &negExif->fGPSImgDirection);
        getRawExifTag("Exif.GPSInfo.GPSMapDatum",         &negExif->fGPSMapDatum);
        getRawExifTag("Exif.GPSInfo.GPSDestLatitudeRef",  &negExif->fGPSDestLatitudeRef);
        getRawExifTag("Exif.GPSInfo.GPSDestLatitude",      negExif->fGPSDestLatitude,  3);
        getRawExifTag("Exif.GPSInfo.GPSDestLongitudeRef", &negExif->fGPSDestLongitudeRef);
        getRawExifTag("Exif.GPSInfo.GPSDestLongitude",     negExif->fGPSDestLongitude, 3);
        getRawExifTag("Exif.GPSInfo.GPSDestBearingRef",   &negExif->fGPSDestBearingRef);
        getRawExifTag("Exif.GPSInfo.GPSDestBearing",   0, &negExif->fGPSDestBearing);
        getRawExifTag("Exif.GPSInfo.GPSDestDistanceRef",  &negExif->fGPSDestDistanceRef);
        getRawExifTag("Exif.GPSInfo.GPSDestDistance",  0, &negExif->fGPSDestDistance);
        getRawExifTag("Exif.GPSInfo.GPSProcessingMethod", &negExif->fGPSProcessingMethod);
        getRawExifTag("Exif.GPSInfo.GPSAreaInformation",  &negExif->fGPSAreaInformation);
        getRawExifTag("Exif.GPSInfo.GPSDateStamp",        &negExif->fGPSDateStamp);
        getRawExifTag("Exif.GPSInfo.GPSDifferential",  0, &negExif->fGPSDifferential);

        // Interoperability IFD
        getRawExifTag("Exif.Iop.InteroperabilityIndex",   &negExif->fInteroperabilityIndex);
        getRawExifTag("Exif.Iop.InteroperabilityVersion",  0, &negExif->fInteroperabilityVersion);

        // CFA pattern (reconstructed from mosaic info)
        const dng_mosaic_info* mosaicinfo = m_negative->GetMosaicInfo();
        if (mosaicinfo != nullptr) {
            negExif->fCFARepeatPatternCols = mosaicinfo->fCFAPatternSize.v;
            negExif->fCFARepeatPatternRows = mosaicinfo->fCFAPatternSize.h;
            for (uint16 c = 0; c < negExif->fCFARepeatPatternCols; c++)
                for (uint16 r = 0; r < negExif->fCFARepeatPatternRows; r++)
                    negExif->fCFAPattern[r][c] = mosaicinfo->fCFAPattern[c][r];
        }

        // Reconstruct LensName from LensInfo when missing
        if (negExif->fLensName.IsEmpty()) {
            dng_urational* li = negExif->fLensInfo;
            std::stringstream lensName;
            lensName.precision(1);
            lensName.setf(std::ios::fixed, std::ios::floatfield);
            if (li[0].IsValid())   lensName << li[0].As_real64();
            if (li[1] != li[2])   lensName << "-" << li[1].As_real64();
            if (lensName.tellp() > 0) lensName << " mm";
            if (li[2].IsValid())   lensName << " f/" << li[2].As_real64();
            if (li[3] != li[2])   lensName << "-" << li[3].As_real64();
            negExif->fLensName.Set_ASCII(lensName.str().c_str());
        }

        // Overwrite with DNG-creation values
        negExif->fDateTime    = dateTimeNow;
        negExif->fSoftware    = appNameVersion;
        negExif->fExifVersion = DNG_CHAR4('0','2','3','0');
    }


    void setXmpFromRaw(const dng_date_time_info& dateTimeNow,
                       const dng_string&         appNameVersion) {
        AutoPtr<dng_xmp> negXmp(new dng_xmp(m_host.Allocator()));

        for (Exiv2::XmpData::const_iterator it = m_RawXmp.begin();
             it != m_RawXmp.end(); ++it) {
            try {
                negXmp->Set(
                    Exiv2::XmpProperties::nsInfo(it->groupName())->ns_,
                    it->tagName().c_str(),
                    it->toString().c_str());
            }
            catch (dng_exception&) {
                std::cerr << "Dropped XMP-entry from raw-file since namespace is unknown: "
                          << "NS: "   << Exiv2::XmpProperties::nsInfo(it->groupName())->ns_
                          << ", path: " << it->tagName()
                          << ", text: " << it->toString() << "\n";
            }
        }

        negXmp->UpdateDateTime(dateTimeNow);
        negXmp->UpdateMetadataDate(dateTimeNow);
        negXmp->SetString(XMP_NS_XAP, "CreatorTool", appNameVersion);
        negXmp->Set(XMP_NS_DC, "format", "image/dng");
        negXmp->SetString(XMP_NS_PHOTOSHOP, "DateCreated",
                          m_negative->GetExif()->fDateTimeOriginal.Encode_ISO_8601());

        m_negative->ResetXMP(negXmp.Release());
    }


    void backupProprietaryData() {
        // Call createDNGPrivateTag() via CRTP so derived overrides are used.
        AutoPtr<dng_memory_stream> DNGPrivateTag(
            static_cast<Derived*>(this)->createDNGPrivateTag());

        if (DNGPrivateTag.Get()) {
            AutoPtr<dng_memory_block> blockPriv(
                DNGPrivateTag->AsMemoryBlock(m_host.Allocator()));
            m_negative->SetPrivateData(blockPriv);
        }
    }


    void buildDNGImage() {
        libraw_image_sizes_t* sizes = &m_RawProcessor->imgdata.sizes;

        unsigned short* rawBuffer = reinterpret_cast<unsigned short*>(
            m_RawProcessor->imgdata.rawdata.raw_image);
        uint32 inputPlanes = 1;

        if (!rawBuffer) {
            rawBuffer = reinterpret_cast<unsigned short*>(
                m_RawProcessor->imgdata.rawdata.color3_image);
            inputPlanes = 3;
        }
        if (!rawBuffer) {
            rawBuffer = reinterpret_cast<unsigned short*>(
                m_RawProcessor->imgdata.rawdata.color4_image);
            inputPlanes = 4;
        }

        uint32 outputPlanes = (inputPlanes == 1)
                              ? 1
                              : m_RawProcessor->imgdata.idata.colors;

        dng_rect bounds(sizes->raw_height, sizes->raw_width);
        dng_simple_image* image = new dng_simple_image(
            bounds, outputPlanes, ttShort, m_host.Allocator());

        dng_pixel_buffer buffer; image->GetPixelBuffer(buffer);
        auto* imageBuffer = static_cast<unsigned short*>(buffer.fData);

        if (inputPlanes == outputPlanes) {
            std::memcpy(imageBuffer, rawBuffer,
                        sizes->raw_height * sizes->raw_width *
                        outputPlanes * sizeof(unsigned short));
        }
        else {
            for (int i = 0; i < (sizes->raw_height * sizes->raw_width); i++) {
                std::memcpy(imageBuffer, rawBuffer,
                            outputPlanes * sizeof(unsigned short));
                imageBuffer += outputPlanes;
                rawBuffer   += inputPlanes;
            }
        }

        AutoPtr<dng_image> castImage(dynamic_cast<dng_image*>(image));
        m_negative->SetStage1Image(castImage);
    }


    // Default createDNGPrivateTag – writes Adobe MakerNote vendor format.
    // Derived classes can shadow this to append extra proprietary blobs.
    dng_memory_stream* createDNGPrivateTag() {
        uint32         mnOffset  = 0;
        dng_string     mnByteOrder;
        long           mnLength  = 0;
        unsigned char* mnBuffer  = nullptr;

        if (!getRawExifTag("Exif.MakerNote.Offset",    0, &mnOffset)    ||
            !getRawExifTag("Exif.MakerNote.ByteOrder",    &mnByteOrder) ||
            !getRawExifTag("Exif.Photo.MakerNote",    &mnLength, &mnBuffer))
            return nullptr;

        bool padding = (mnLength & 0x01) == 0x01;

        dng_memory_stream* streamPriv = new dng_memory_stream(m_host.Allocator());
        streamPriv->SetBigEndian();

        streamPriv->Put("Adobe", 5);
        streamPriv->Put_uint8(0x00);
        streamPriv->Put("MakN", 4);
        streamPriv->Put_uint32(static_cast<uint32>(mnLength)
                               + mnByteOrder.Length() + 4
                               + (padding ? 1 : 0));
        streamPriv->Put(mnByteOrder.Get(), mnByteOrder.Length());
        streamPriv->Put_uint32(mnOffset);
        streamPriv->Put(mnBuffer, static_cast<uint32>(mnLength));
        if (padding) streamPriv->Put_uint8(0x00);

        delete[] mnBuffer;
        return streamPriv;
    }


protected:

    // -----------------------------------------------------------------------
    // Constructor – initialises LibRaw/Exiv2 state and creates the dng_negative.
    // Protected: only called by derived-class constructors.
    // -----------------------------------------------------------------------
    BaseProcessor(dng_host&                    host,
                  std::unique_ptr<LibRaw>      rawProcessor,
                  Exiv2::Image::UniquePtr      rawImage)
        : m_RawProcessor(std::move(rawProcessor))
        , m_RawImage(std::move(rawImage))
        , m_RawExif(m_RawImage->exifData())
        , m_RawXmp(m_RawImage->xmpData())
        , m_host(host)
        , m_negative(std::unique_ptr<dng_negative>(m_host.Make_dng_negative()))
    {
        // Create an empty negative; derived constructors may immediately
        // populate it (e.g. DNGprocessor re-parses the DNG into m_negative).
    }

    // Non-copyable / non-movable (owns unique resources).
    BaseProcessor(const BaseProcessor&)            = delete;
    BaseProcessor& operator=(const BaseProcessor&) = delete;


    // -----------------------------------------------------------------------
    // Protected state – accessible to derived classes
    // -----------------------------------------------------------------------
    std::unique_ptr<LibRaw>      m_RawProcessor;
    Exiv2::Image::UniquePtr      m_RawImage;
    Exiv2::ExifData              m_RawExif;
    Exiv2::XmpData               m_RawXmp;
    dng_host&                    m_host;
    std::unique_ptr<dng_negative> m_negative;


    // -----------------------------------------------------------------------
    // Exif helper functions (same signatures as before)
    // -----------------------------------------------------------------------

    bool getInterpretedRawExifTag(const char* exifTagName,
                                  int32 component, uint32* value) {
        auto it = m_RawExif.findKey(Exiv2::ExifKey(exifTagName));
        if (it == m_RawExif.end()) return false;
        std::stringstream interpretedValue; it->write(interpretedValue, &m_RawExif);
        uint32 tmp;
        for (int i = 0; (i <= component) && !interpretedValue.fail(); i++)
            interpretedValue >> tmp;
        if (interpretedValue.fail()) return false;
        *value = tmp;
        return true;
    }

    bool getRawExifTag(const char* exifTagName, dng_string* value) {
        auto it = m_RawExif.findKey(Exiv2::ExifKey(exifTagName));
        if (it == m_RawExif.end()) return false;
        value->Set_ASCII((it->print(&m_RawExif)).c_str());
        value->TrimLeadingBlanks(); value->TrimTrailingBlanks();
        return true;
    }

    bool getRawExifTag(const char* exifTagName, dng_date_time_info* value) {
        auto it = m_RawExif.findKey(Exiv2::ExifKey(exifTagName));
        if (it == m_RawExif.end()) return false;
        dng_date_time dt; dt.Parse((it->print(&m_RawExif)).c_str());
        value->SetDateTime(dt);
        return true;
    }

    bool getRawExifTag(const char* exifTagName,
                       int32 component, dng_srational* rational) {
        auto it = m_RawExif.findKey(Exiv2::ExifKey(exifTagName));
        if (it == m_RawExif.end() || it->count() < (component + 1)) return false;
        Exiv2::Rational r = (*it).toRational(component);
        *rational = dng_srational(r.first, r.second);
        return true;
    }

    bool getRawExifTag(const char* exifTagName,
                       int32 component, dng_urational* rational) {
        auto it = m_RawExif.findKey(Exiv2::ExifKey(exifTagName));
        if (it == m_RawExif.end() || it->count() < (component + 1)) return false;
        Exiv2::URational r = (*it).toRational(component);
        *rational = dng_urational(r.first, r.second);
        return true;
    }

    bool getRawExifTag(const char* exifTagName,
                       int32 component, uint32* value) {
        auto it = m_RawExif.findKey(Exiv2::ExifKey(exifTagName));
        if (it == m_RawExif.end() || it->count() < (component + 1)) return false;
        *value = static_cast<uint32>(it->toInt64(component));
        return true;
    }

    int getRawExifTag(const char* exifTagName,
                      uint32* valueArray, int32 maxFill) {
        auto it = m_RawExif.findKey(Exiv2::ExifKey(exifTagName));
        if (it == m_RawExif.end()) return 0;
        int len = std::min(maxFill, static_cast<int32>(it->count()));
        for (int i = 0; i < len; i++)
            valueArray[i] = static_cast<uint32>(it->toInt64(i));
        return len;
    }

    int getRawExifTag(const char* exifTagName,
                      int16* valueArray, int32 maxFill) {
        auto it = m_RawExif.findKey(Exiv2::ExifKey(exifTagName));
        if (it == m_RawExif.end()) return 0;
        int len = std::min(maxFill, static_cast<int32>(it->count()));
        for (int i = 0; i < len; i++)
            valueArray[i] = static_cast<int16>(it->toInt64(i));
        return len;
    }

    int getRawExifTag(const char* exifTagName,
                      dng_urational* valueArray, int32 maxFill) {
        auto it = m_RawExif.findKey(Exiv2::ExifKey(exifTagName));
        if (it == m_RawExif.end()) return 0;
        int len = std::min(maxFill, static_cast<int32>(it->count()));
        for (int i = 0; i < len; i++) {
            Exiv2::URational r = (*it).toRational(i);
            valueArray[i] = dng_urational(r.first, r.second);
        }
        return len;
    }

    bool getRawExifTag(const char* exifTagName,
                       long* size, unsigned char** data) {
        auto it = m_RawExif.findKey(Exiv2::ExifKey(exifTagName));
        if (it == m_RawExif.end()) return false;
        *data = new unsigned char[(*it).size()];
        *size = (*it).size();
        (*it).copy(reinterpret_cast<Exiv2::byte*>(*data), Exiv2::bigEndian);
        return true;
    }
};