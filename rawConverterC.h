#ifndef RawConverterC_h
#define RawConverterC_h

#ifdef __cplusplus
extern "C" {
#endif

// Opaque pointer to hide C++ implementation
typedef void* RawConverterHandle;

// Functions for raw conversion
void raw2dng(RawConverterHandle handle, const char* rawFilename, const char* outFilename, const char* dcpFilename, int embedOriginal);
void raw2tiff(RawConverterHandle handle, const char* rawFilename, const char* outFilename, const char* dcpFilename);
void raw2jpeg(RawConverterHandle handle, const char* rawFilename, const char* outFilename, const char* dcpFilename);

// Callback registration
typedef void (*PublisherCallback)(const char*);
void registerPublisher(RawConverterHandle handle, PublisherCallback callback);

// Handle management
RawConverterHandle createRawConverter();
void destroyRawConverter(RawConverterHandle handle);

#ifdef __cplusplus
}
#endif

#endif