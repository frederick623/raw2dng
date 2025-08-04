
#include "RawConverterC.h"
#include "raw2dng/RawConverter.hpp" // Your original C++ header
#include <string>
#include <functional>

// Assuming your C++ implementation is in a class or namespace
// If it's just free functions, you can call them directly
struct RawConverter {
    std::function<void(const char*)> publisher;
};

RawConverterHandle createRawConverter() {
    return new RawConverter();
}

void destroyRawConverter(RawConverterHandle handle) {
    if (handle) {
        delete static_cast<RawConverter*>(handle);
    }
}

void raw2dng(RawConverterHandle handle, const char* rawFilename, const char* outFilename, const char* dcpFilename, int embedOriginal) {
    if (handle && rawFilename && outFilename && dcpFilename) {
        ::raw2dng(rawFilename, outFilename, dcpFilename, embedOriginal != 0);
    }
}

void raw2tiff(RawConverterHandle handle, const char* rawFilename, const char* outFilename, const char* dcpFilename) {
    if (handle && rawFilename && outFilename && dcpFilename) {
        ::raw2tiff(rawFilename, outFilename, dcpFilename);
    }
}

void raw2jpeg(RawConverterHandle handle, const char* rawFilename, const char* outFilename, const char* dcpFilename) {
    if (handle && rawFilename && outFilename && dcpFilename) {
        ::raw2jpeg(rawFilename, outFilename, dcpFilename);
    }
}

void registerPublisher(RawConverterHandle handle, PublisherCallback callback) {
    if (handle && callback) {
        auto* converter = static_cast<RawConverter*>(handle);
        converter->publisher = callback;
        ::registerPublisher([callback](const std::string& message) {
            callback(message.c_str());
        });
    }
}