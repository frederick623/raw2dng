//
//  Raw2DNGBridge.mm
//  Raw2DNG
//
//  Objective-C++ implementation that calls the C++ library
//

#import "Raw2DNGBridge.h"
#include <string>

// Forward declare the C++ function from raw2dng library
extern "C" {
    void raw2dng(const std::string& rawFilename, 
                 const std::string& outFilename, 
                 const std::string& dcpFilename);
}

@implementation Raw2DNGBridge

+ (BOOL)convert:(NSString *)rawPath
     outputPath:(NSString *)outputPath
        dcpPath:(NSString *)dcpPath {
    
    if (!rawPath || !outputPath) {
        NSLog(@"Error: rawPath or outputPath is nil");
        return NO;
    }
    
    @try {
        // Convert NSString to std::string
        std::string rawPathStr = [rawPath UTF8String];
        std::string outputPathStr = [outputPath UTF8String];
        std::string dcpPathStr = dcpPath ? [dcpPath UTF8String] : "";
        
        // Call the C++ conversion function
        raw2dng(rawPathStr, outputPathStr, dcpPathStr);
        
        // Check if output file was created
        NSFileManager *fileManager = [NSFileManager defaultManager];
        if ([fileManager fileExistsAtPath:outputPath]) {
            NSLog(@"Successfully converted: %@", [rawPath lastPathComponent]);
            return YES;
        } else {
            NSLog(@"Conversion failed - output file not created: %@", [rawPath lastPathComponent]);
            return NO;
        }
    }
    @catch (NSException *exception) {
        NSLog(@"Exception during conversion: %@", exception.reason);
        return NO;
    }
    @catch (...) {
        NSLog(@"Unknown exception during conversion");
        return NO;
    }
}

+ (NSString *)version {
    return @"Raw2DNG iOS v1.0";
}

@end
