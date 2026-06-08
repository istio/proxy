import Foundation

@objc
public class MixedAnswerSwift: NSObject {

    public
    static func swiftMixedAnswer() -> String {
        "\(MixedAnswerObjc.mixedAnswerObjc() ?? "invalid")_swiftMixedAnswer"
    }

    @objc
    public
    static func swiftToObjcMixedAnswer() -> String {
        "swiftToObjcMixedAnswer"
    }
}
