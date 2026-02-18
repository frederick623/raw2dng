//
//  Raw2DNGBridge.h
//  Raw2DNG
//
//  Objective-C bridge to C++ raw2dng library
//

#import <Foundation/Foundation.h>

NS_ASSUME_NONNULL_BEGIN

@interface Raw2DNGBridge : NSObject

/// Convert a RAW file to DNG format
/// @param rawPath Path to the input RAW file
/// @param outputPath Path for the output DNG file
/// @param dcpPath Path to DCP (DNG Camera Profile) file, or empty string for default
/// @return YES if conversion succeeded, NO otherwise
+ (BOOL)convert:(NSString *)rawPath
     outputPath:(NSString *)outputPath
        dcpPath:(NSString *)dcpPath;

/// Get library version information
+ (NSString *)version;

@end

NS_ASSUME_NONNULL_END
