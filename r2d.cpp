
#include <iostream>
#include <stdexcept>

#include "rawConverter/rawConverter.h"

struct Raw2Dng
{
    void raw2dng(const std::string& rawFilename, const std::string& outFilename, const std::string& dcpFilename)
    {
        converter.openRawFile(rawFilename);
        converter.buildNegative(dcpFilename);
        // if (embedOriginal) converter.embedRaw(rawFilename);
        converter.renderImage();
        converter.renderPreviews();
        converter.writeDng(outFilename);
    }

    void raw2tiff(const std::string& rawFilename, const std::string& outFilename, const std::string& dcpFilename)
    {
        converter.openRawFile(rawFilename);
        converter.buildNegative(dcpFilename);
        // if (embedOriginal) converter.embedRaw(rawFilename);
        converter.renderImage();
        converter.renderPreviews();
        converter.writeTiff(outFilename);
    }

    void raw2jpeg(const std::string& rawFilename, const std::string& outFilename, const std::string& dcpFilename)
    {
        converter.openRawFile(rawFilename);
        converter.buildNegative(dcpFilename);
        // if (embedOriginal) converter.embedRaw(rawFilename);
        converter.renderImage();
        converter.renderPreviews();
        converter.writeJpeg(outFilename);
    }

private:
    RawConverter converter;
};

int main(int argc, const char* argv []) {  
    if (argc == 1) {
        std::cerr << "\n"
                     "raw2dng - DNG converter\n"
                     "Usage: " << argv[0] << " [options] <rawfile>\n"
                     "Valid options:\n"
                     "  -dcp <filename>      use adobe camera profile\n"
                     "  -j                   convert to JPEG instead of DNG\n"
                     "  -t                   convert to TIFF instead of DNG\n"
                     "  -o <filename>        specify output filename\n\n";
        return -1;
    }

    // -----------------------------------------------------------------------------------------
    // Parse command line

    std::string outFilename;
    std::string dcpFilename;
    bool isJpeg = false, isTiff = false;

    int index;
    for (index = 1; index < argc && argv [index][0] == '-'; index++) {
        std::string option = &argv[index][1];
        if (0 == strcmp(option.c_str(), "o"))   outFilename = std::string(argv[++index]);
        if (0 == strcmp(option.c_str(), "dcp")) dcpFilename = std::string(argv[++index]);
        if (0 == strcmp(option.c_str(), "j"))   isJpeg = true;
        if (0 == strcmp(option.c_str(), "t"))   isTiff = true;
    }

    if (index == argc) {
        std::cerr << "No file specified\n";
        return 1;
    }

    std::string rawFilename(argv[index++]);

    // set output filename: if not given in command line, replace raw file extension
    if (outFilename.empty()) {
        outFilename.assign(rawFilename, 0, rawFilename.find_last_of("."));
        if (isJpeg)      outFilename.append(".jpg");
        else if (isTiff) outFilename.append(".tif");
        else             outFilename.append(".dng");
    }

    // -----------------------------------------------------------------------------------------
    // Call the conversion function

    std::cout << "Starting conversion: \"" << rawFilename << "\n";
    std::time_t startTime = std::time(NULL);

    try {
        Raw2Dng conv;
        if (isJpeg)      conv.raw2jpeg(rawFilename, outFilename, dcpFilename);
        else if (isTiff) conv.raw2tiff(rawFilename, outFilename, dcpFilename);
        else             conv.raw2dng (rawFilename, outFilename, dcpFilename);
    }
    catch (std::exception& e) {
        std::cerr << "--> Error! (" << e.what() << ")\n\n";
        return -1;
    }

    std::cout << "--> Done (" << std::difftime(std::time(NULL), startTime) << " seconds)\n\n";

    return 0;
}
