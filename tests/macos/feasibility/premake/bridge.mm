#import <Foundation/Foundation.h>

const char* HazelMacOSFoundationVersion() {
    static NSString* version = [[NSProcessInfo processInfo] operatingSystemVersionString];
    return [version UTF8String];
}
