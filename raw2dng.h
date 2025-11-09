
#include "raw2dng/rawConverter.h"

struct Raw2Dng
{

    void raw2dng(const std::string& rawFilename, const std::string& outFilename, const std::string& dcpFilename);
    void raw2tiff(const std::string& rawFilename, const std::string& outFilename, const std::string& dcpFilename);
    void raw2jpeg(const std::string& rawFilename, const std::string& outFilename, const std::string& dcpFilename);

private:
    RawConverter converter;
};
