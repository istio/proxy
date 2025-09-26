import MixedAnswer

class SwiftLibDependingOnMixedLib {
    static func callSwiftMixedAnswer() -> String {
        MixedAnswerSwift.swiftMixedAnswer()
    }

    static func callObjcMixedAnswer() -> String? {
        MixedAnswerObjc.mixedAnswerObjc()
    }
}
