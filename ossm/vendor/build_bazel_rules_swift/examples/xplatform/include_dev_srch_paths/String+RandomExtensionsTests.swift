import StringHelpers
import XCTest

class StringRandomTests: XCTestCase {
    func test_random() {
        do_random_test(mode: .upperAlpha)
        do_random_test(mode: .lowerAlpha)
        do_random_test(mode: .upperAlphaNumeric)
        do_random_test(mode: .lowerAlphaNumeric)
        do_random_test(mode: .alpha)
        do_random_test(mode: .alphaNumeric)
    }

    func do_random_test(mode: String.RandomStringMode, length: Int = 10) {
        let result = String.random(length: length, mode: mode)
        assertThat(result.count).isEqualTo(length)
        XCTAssertEqual(result.count, length)
    }
}
