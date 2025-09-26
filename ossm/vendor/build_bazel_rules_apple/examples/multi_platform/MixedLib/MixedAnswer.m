#import "examples/multi_platform/MixedLib/MixedAnswer.h"
#import "examples/multi_platform/MixedLib/MixedAnswer-Swift.h"

@implementation MixedAnswerObjc

+ (NSString *)mixedAnswerObjc {
    return [NSString stringWithFormat:@"%@_%@", @"mixedAnswerObjc", [MixedAnswerSwift swiftToObjcMixedAnswer]];
}

@end
