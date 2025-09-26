public extension String {
    static let lowerAlpha = "abcdefghijklmnopqrstuvwxyz"
    static let upperAlpha: String = .lowerAlpha.uppercased()
    static let numeric = "0123456789"
    static let lowerAlphaNumeric: String = .lowerAlpha + .numeric
    static let upperAlphaNumeric: String = .upperAlpha + .numeric
    static let allAlpha: String = .lowerAlpha + .upperAlpha
    static let allAlphaNumeric: String = .allAlpha + .numeric

    /// Specifies the type of random string that should be generated.
    enum RandomStringMode {
        case upperAlpha
        case lowerAlpha
        case upperAlphaNumeric
        case lowerAlphaNumeric
        case alpha
        case alphaNumeric

        /// Returns the characters that should be used for the mode.
        public var letters: String {
            switch self {
            case .upperAlpha:
                return .upperAlpha
            case .lowerAlpha:
                return .lowerAlpha
            case .upperAlphaNumeric:
                return .upperAlphaNumeric
            case .lowerAlphaNumeric:
                return .lowerAlphaNumeric
            case .alpha:
                return .allAlpha
            case .alphaNumeric:
                return .allAlphaNumeric
            }
        }
    }

    /// Generates a random string with the specified length and the specified contents.
    static func random(length: Int, mode: RandomStringMode) -> String {
        return String((0 ..< length).map { _ in mode.letters.randomElement()! })
    }
}
