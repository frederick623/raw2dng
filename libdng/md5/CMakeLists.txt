# =======================================================
# MD5 source code.

ADD_LIBRARY( md5 STATIC MD5.cpp )
TARGET_INCLUDE_DIRECTORIES( md5 INTERFACE ${CMAKE_CURRENT_SOURCE_DIR} )

# Force arm64 (you can also add x86_64 here for a universal lib)
target_compile_options(md5 PRIVATE -arch arm64)
target_link_options(md5 PRIVATE -arch arm64)

# Optional: make the output name match the old lib (libmd5.a)
set_target_properties(md5 PROPERTIES OUTPUT_NAME md5)