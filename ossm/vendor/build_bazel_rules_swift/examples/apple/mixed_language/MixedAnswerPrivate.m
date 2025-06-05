#import "examples/apple/mixed_language/MixedAnswerPrivate.h"
#import "examples/apple/mixed_language/MixedAnswer-Swift.h"

@implementation MixedAnswerPrivateObjc

+ (NSString *)mixedAnswerPrivateObjc {
    return [NSString stringWithFormat:@"%@_%@", @"mixedAnswerPrivateObjc", [MixedAnswerSwift swiftToObjcMixedAnswer]];
}

@end
