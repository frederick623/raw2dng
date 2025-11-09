
#include "raw2dng.h"

void Raw2Dng::raw2dng(const std::string& rawFilename, const std::string& outFilename, const std::string& dcpFilename) {
    converter.openRawFile(rawFilename);
    converter.buildNegative(dcpFilename);
    // if (embedOriginal) converter.embedRaw(rawFilename);
    converter.renderImage();
    converter.renderPreviews();
    converter.writeDng(outFilename);
}


void Raw2Dng::raw2tiff(const std::string& rawFilename, const std::string& outFilename, const std::string& dcpFilename) {
    converter.openRawFile(rawFilename);
    converter.buildNegative(dcpFilename);
    converter.renderImage();
    converter.renderPreviews();
    converter.writeTiff(outFilename);
}


void Raw2Dng::raw2jpeg(const std::string& rawFilename, const std::string& outFilename, const std::string& dcpFilename) {
    converter.openRawFile(rawFilename);
    converter.buildNegative(dcpFilename);
    converter.renderImage();
    converter.writeJpeg(outFilename);
}

