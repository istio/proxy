#import <objc/runtime.h>
#import <Foundation/Foundation.h>
#import <XCTest/XCTest.h>

static NSString *kSourceRoot;

NSURL* remapFileUrl (NSURL *fileURL) {
    if ([fileURL.path hasPrefix:kSourceRoot]) {
        return fileURL;
    }

    return [NSURL fileURLWithPath:
        [NSString stringWithFormat:@"%@/%@", kSourceRoot, fileURL.relativePath]
    ];
}

@implementation XCTSourceCodeLocation (FixRelativePaths)

+ (void)load {
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        kSourceRoot = [NSProcessInfo processInfo]
            .environment[@"BUILD_WORKSPACE_DIRECTORY"];
        if (!kSourceRoot) {
            NSLog(@"warning: The 'BUILD_WORKSPACE_DIRECTORY' environment "
                "variable was not set. Test issue navigation might not work.");
            return;
        }
        if (![kSourceRoot hasPrefix:@"/"]) {
            NSLog(@"warning: The 'BUILD_WORKSPACE_DIRECTORY' was not an "
                "absolute path. Test issue navigation might not work.");
            return;
        }

        Class class = [XCTSourceCodeLocation class];

        SEL originalSelector = @selector(initWithFileURL:lineNumber:);
        SEL swizzledSelector = @selector(xxx_initWithRelativeFileURL:lineNumber:);

        Method originalMethod = class_getInstanceMethod(class, originalSelector);
        Method swizzledMethod = class_getInstanceMethod(class, swizzledSelector);

        method_exchangeImplementations(originalMethod, swizzledMethod);
    });
}

- (instancetype)xxx_initWithRelativeFileURL:(NSURL *)fileURL
                                 lineNumber:(NSInteger)lineNumber
{
    // Not recursive because of swizzling
    return [self xxx_initWithRelativeFileURL:remapFileUrl(fileURL)
                                  lineNumber:lineNumber];
}

@end
