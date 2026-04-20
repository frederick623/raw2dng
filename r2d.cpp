
#include <cstring>
#include <chrono>
#include <iostream>
#include <stdexcept>
#include <vector>

#include "rawConverter/rawConverter.h"

struct Raw2Dng
{
    static void raw2dng(const std::string& rawFilename, const std::string& outFilename, const std::string& dcpFilename)
    {
        RawConverter converter({rawFilename}, dcpFilename);
        converter.writeDng(rawFilename, outFilename);
    }

    static void raw2tiff(const std::string& rawFilename, const std::string& outFilename, const std::string& dcpFilename)
    {
        RawConverter converter({rawFilename}, dcpFilename);
        converter.writeTiff(rawFilename, outFilename);
    }

    static void raw2jpeg(const std::string& rawFilename, const std::string& outFilename, const std::string& dcpFilename)
    {
        RawConverter converter({rawFilename}, dcpFilename);
        converter.writeJpeg(rawFilename, outFilename);
    }

};

int main(int argc, const char* argv []) {  
    if (argc == 1) {
        std::cerr << "\n"
                     "raw2dng - DNG converter\n"
                     "Usage: " << argv[0] << " [options] <rawfile> ...\n"
                     "Valid options:\n"
                     "  -dcp <filename>      use adobe camera profile\n"
                     "  -j                   convert to JPEG instead of DNG\n"
                     "  -t                   convert to TIFF instead of DNG\n"
                     "  -o <filename>        specify output filename\n\n";
        return -1;
    }

    // -----------------------------------------------------------------------------------------
    // Parse command line

    std::vector<std::string> rawFilenames;
    std::string outFilename;
    std::string dcpFilename;
    bool isJpeg = false, isTiff = false;

    if (argc==1) {
        std::cerr << "No file specified\n";
        return 1;
    }

    int index;
    for (index = 1; index < argc ; index++) {
        if (argv [index][0] == '-')
        {
            std::string option = &argv[index][1];
            if (0 == strcmp(option.c_str(), "o"))   outFilename = std::string(argv[++index]);
            if (0 == strcmp(option.c_str(), "dcp")) dcpFilename = std::string(argv[++index]);
            if (0 == strcmp(option.c_str(), "j"))   isJpeg = true;
            if (0 == strcmp(option.c_str(), "t"))   isTiff = true;
        }
        else rawFilenames.emplace_back(argv[index]);
    }

    for (const auto& rawFilename : rawFilenames)
    {
        // set output filename: if not given in command line, replace raw file extension
        if (rawFilenames.size()>1 or outFilename.empty()) {
            outFilename.assign(rawFilename, 0, rawFilename.find_last_of("."));
            if (isJpeg)      outFilename.append(".jpg");
            else if (isTiff) outFilename.append(".tif");
            else             outFilename.append(".dng");
        }

        // -----------------------------------------------------------------------------------------
        // Call the conversion function

        std::cout << "Starting conversion: \"" << rawFilename << "\n";
        auto startTime = std::chrono::steady_clock::now();

        try {
            if (isJpeg)      Raw2Dng::raw2jpeg(rawFilename, outFilename, dcpFilename);
            else if (isTiff) Raw2Dng::raw2tiff(rawFilename, outFilename, dcpFilename);
            else             Raw2Dng::raw2dng (rawFilename, outFilename, dcpFilename);
        }
        catch (std::exception& e) {
            std::cerr << "--> Error! (" << e.what() << ")\n\n";
            return -1;
        }

        std::cout << "--> Done (" << std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now()-startTime) << " seconds)\n\n";

    }
    return 0;
}
