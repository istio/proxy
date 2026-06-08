import XCTest

class PassFailTests: XCTestCase {

    func test_pass() {
        TestHelper.Pass()
    }

    func test_fail() {
        TestHelper.ExpectFailureIfNeeded()
    }
}
