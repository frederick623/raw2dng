# raw2dng

Linux utility for converting raw photo files into DNG, TIFF and JPEG formats.

Original repo (Fimagena/raw2dng) had been unmaintained for a decade while it cannot be built any more.

Current repo fixed the build issue and add latest Adobe DNG SDK.

All major dependencies (dng-sdk, exiv2, xmp-sdk, libraw, libjxl) are incl and built static so that this can be built in iOS

**Compile:** `cmake`, `make`