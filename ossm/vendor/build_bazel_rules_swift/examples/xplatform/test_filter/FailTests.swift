import XCTest

class FailTests: XCTestCase {

    func test_fail() {
        TestHelper.ExpectFailureIfNeeded()
    }
}
