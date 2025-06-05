#import "examples/apple/mixed_language/MixedAnswer.h"
#import "examples/apple/mixed_language/MixedAnswerPrivate.h"
#import "examples/apple/mixed_language/MixedAnswer-Swift.h"

@implementation MixedAnswerObjc

+ (NSString *)mixedAnswerObjc {
    return [NSString stringWithFormat:@"%@_%@ and %@", @"mixedAnswerObjc", [MixedAnswerSwift swiftToObjcMixedAnswer], [MixedAnswerPrivateObjc mixedAnswerPrivateObjc]];
}

@end
