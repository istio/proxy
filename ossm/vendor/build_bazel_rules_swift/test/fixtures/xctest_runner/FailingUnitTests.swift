import XCTest

final class FailingUnitTests: XCTestCase {
  func testFail() {
    XCTAssertEqual(0, 1, "should fail")
  }
}
