#ifndef RAW_CONVERTER_C_H
#define RAW_CONVERTER_C_H

#ifdef __cplusplus
extern "C" {
#endif

/* Opaque handle to RawConverter object */
typedef void* RawConverterHandle;

/* Create a new RawConverter instance */
RawConverterHandle RawConverter_Create();

/* Destroy a RawConverter instance */
void RawConverter_Destroy(RawConverterHandle handle);

/* Open a raw file */
void RawConverter_OpenRawFile(RawConverterHandle handle, const char* rawFilename);

/* Build negative using a DCP file */
void RawConverter_BuildNegative(RawConverterHandle handle, const char* dcpFilename);

/* Embed a raw file */
void RawConverter_EmbedRaw(RawConverterHandle handle, const char* rawFilename);

/* Render the image */
void RawConverter_RenderImage(RawConverterHandle handle);

/* Render previews */
void RawConverter_RenderPreviews(RawConverterHandle handle);

/* Write output as DNG */
void RawConverter_WriteDng(RawConverterHandle handle, const char* outFilename);

/* Write output as TIFF */
void RawConverter_WriteTiff(RawConverterHandle handle, const char* outFilename);

/* Write output as JPEG */
void RawConverter_WriteJpeg(RawConverterHandle handle, const char* outFilename);

#ifdef __cplusplus
}
#endif

#endif /* RAW_CONVERTER_C_H */