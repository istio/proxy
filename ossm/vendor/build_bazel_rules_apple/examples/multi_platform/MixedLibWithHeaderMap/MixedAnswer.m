#import <MixedAnswer/MixedAnswer.h>
#import <MixedAnswer/MixedAnswer-Swift.h>

@implementation MixedAnswerObjc

+ (NSString *)mixedAnswerObjc {
    return [NSString stringWithFormat:@"%@_%@", @"mixedAnswerObjc", [MixedAnswerSwift swiftToObjcMixedAnswer]];
}

@end
