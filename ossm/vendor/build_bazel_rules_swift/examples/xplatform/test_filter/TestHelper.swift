import XCTest

enum TestHelper {

    static func ExpectFailureIfNeeded() {
        #if canImport(Darwin)
        let options: XCTExpectedFailure.Options = .init()
        options.isEnabled = ProcessInfo.processInfo.environment["EXPECT_FAILURE"] == "TRUE"
        XCTExpectFailure("Expected failure", options: options) {
            Fail()
        }
        #else
        // https://github.com/swiftlang/swift-corelibs-xctest/issues/348
        if ProcessInfo.processInfo.environment["EXPECT_FAILURE"] != "TRUE" {
            Fail()
        }
        #endif
    }

    static func Pass() {
        XCTAssertTrue(true)
    }

    private static func Fail() {
        XCTFail("Fail")
    }
}
