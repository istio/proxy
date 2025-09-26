import MixedAnswer

public class SwiftLibDependingOnMixedLib {
    public static func callSwiftMixedAnswer() -> String {
        MixedAnswerSwift.swiftMixedAnswer()
    }

    public static func callObjcMixedAnswer() -> String? {
        MixedAnswerObjc.mixedAnswerObjc()
    }
}
