# raw2dng
Linux utility for converting raw photo files into DNG, TIFF and JPEG formats.

Original repo (Fimagena/raw2dng) had been unmaintained for a decade while it cannot be built any more.

Current repo fixed the build issue and add latest Adobe DNG SDK.

**Compile:** `cmake`, `make`, `make install`

**Dependencies:**
 - libexiv2 (tested with v0.25)
 - libraw (tested with 0.17.1)
 - Adobe's DNG and XMP SDKs (included in source tree - v1.7)
 - libexpat
 - libjpeg
 - zlib
