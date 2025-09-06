/* raw_converter_c.cpp – C++ implementation of the C wrapper */
#include "rawConverterC.h"      // the C header
#include "rawConverter.h"      // the original C++ class

#ifdef __cplusplus
extern "C" {
#endif

RawConverterHandle RawConverter_Create(void) {
    return new RawConverter();
}

void RawConverter_Destroy(RawConverterHandle rc) {
    delete static_cast<RawConverter*>(rc);
}

void RawConverter_openRawFile(RawConverterHandle rc, const char* rawFilename) {
    if (rc) static_cast<RawConverter*>(rc)->openRawFile(rawFilename);
}

void RawConverter_buildNegative(RawConverterHandle rc, const char* dcpFilename) {
    if (rc) static_cast<RawConverter*>(rc)->buildNegative(dcpFilename);
}

void RawConverter_EmbedRaw(RawConverterHandle rc, const char* rawFilename) {
    if (rc) static_cast<RawConverter*>(rc)->embedRaw(rawFilename);
}

void RawConverter_RenderImage(RawConverterHandle rc) {
    if (rc) static_cast<RawConverter*>(rc)->renderImage();
}

void RawConverter_RenderPreviews(RawConverterHandle rc) {
    if (rc) static_cast<RawConverter*>(rc)->renderPreviews();
}

void RawConverter_WriteDng(RawConverterHandle rc, const char* dcpFilename) {
    if (rc) static_cast<RawConverter*>(rc)->writeDng(dcpFilename);
}

void RawConverter_WriteTiff(RawConverterHandle rc, const char* rawFilename) {
    if (rc) static_cast<RawConverter*>(rc)->writeTiff(rawFilename);
}

void RawConverter_WriteJpeg(RawConverterHandle rc, const char* dcpFilename) {
    if (rc) static_cast<RawConverter*>(rc)->writeJpeg(dcpFilename);
}

#ifdef __cplusplus
}
#endif