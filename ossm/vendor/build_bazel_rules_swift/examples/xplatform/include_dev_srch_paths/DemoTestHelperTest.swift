@testable import TestHelpers
import XCTest

class DemoTestHelperTest: XCTestCase {
    func test_assertThat_isEqualTo() {
        // To demonstrate a failure, change the expected value to "goodbye".
        assertThat("hello").isEqualTo("hello")
    }
}
