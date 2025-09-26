/// This class is documented.
public class SomeClass {
  /// This method is documented.
  ///
  /// - Parameter count: This parameter is documented.
  /// - Returns: This return value is documented.
  public func someMethod(someParameter count: Int) -> String {
    return String(repeating: "someString", count: count)
  }
}

extension String {
  /// This method is documented, and it's on an extension.
  ///
  /// - Returns: This return value is documented.
  public func someExtensionMethod() -> String {
    return self
  }
}
