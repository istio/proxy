#if canImport(XCTest)
    import XCTest

    public class Subject<T> {
        public let actual: T

        public init(actual: T) {
            self.actual = actual
        }
    }

    /// Return a subject for the target.
    public func assertThat<T>(_ actual: T) -> Subject<T> {
        return Subject(actual: actual)
    }

    public extension Subject where T: Equatable {
        func isEqualTo(_ expected: T) {
            XCTAssertEqual(actual, expected)
        }
    }

    public extension XCTestCase {
        func assertThat<T>(_ actual: T) -> Subject<T> {
            return Subject(actual: actual)
        }
    }
#endif
