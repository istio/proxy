import MixedAnswer

class SwiftLibDependingOnMixedLibWithHeaderMap {
    static func callSwiftMixedAnswer() -> String {
        MixedAnswerSwift.swiftMixedAnswer()
    }

    static func callObjcMixedAnswer() -> String? {
        MixedAnswerObjc.mixedAnswerObjc()
    }
}
