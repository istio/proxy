import XCTest

final class PassingUnitTests: XCTestCase {
  func testPass() {
    let result = 1 + 1
    XCTAssertEqual(result, 2, "should pass")
  }

  func testSrcdirSet() {
    XCTAssertNotNil(ProcessInfo.processInfo.environment["TEST_SRCDIR"])
  }

  func testUndeclaredOutputsSet() {
    XCTAssertNotNil(ProcessInfo.processInfo.environment["TEST_UNDECLARED_OUTPUTS_DIR"])
  }
}
