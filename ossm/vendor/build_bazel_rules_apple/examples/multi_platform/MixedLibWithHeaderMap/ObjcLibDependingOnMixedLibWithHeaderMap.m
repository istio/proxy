@import Foundation;

#import <MixedAnswer/MixedAnswer.h>

#import "ObjcLibDependingOnMixedLibWithHeaderMap.h"

@implementation ObjcLibDependingOnMixedLibWithHeaderMap

+ (void)doSomething {
  MixedAnswerObjc *objcAnswer __unused = [[MixedAnswerObjc alloc] init];
  return;
}

@end
