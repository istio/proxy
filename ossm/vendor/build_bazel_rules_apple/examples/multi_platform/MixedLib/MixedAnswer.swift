import Foundation

@objc
public class MixedAnswerSwift: NSObject {

    public
    static func swiftMixedAnswer() -> String {
        "\(MixedAnswerObjc.mixedAnswerObjc() ?? "invalid")_swiftMixedAnswer"
    }

    public
    static func swiftMixedAnswerPrivate() -> String {
        "\(MixedAnswerPrivateObjc.mixedAnswerPrivateObjc() ?? "invalid")_swiftPrivateMixedAnswer"
    }

    @objc
    public
    static func swiftToObjcMixedAnswer() -> String {
        "swiftToObjcMixedAnswer"
    }
}
