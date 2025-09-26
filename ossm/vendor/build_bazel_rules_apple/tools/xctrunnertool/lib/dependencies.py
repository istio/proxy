#!/usr/bin/env python3

"""
List of dependencies (frameworks, private frameworks, dylibs, etc.)
to copy to the test bundle.
"""

FRAMEWORK_DEPS = [
    "XCTest.framework",
    "Testing.framework",  # Xcode 16+
]

PRIVATE_FRAMEWORK_DEPS = [
    "XCTAutomationSupport.framework",  # Xcode 15+
    "XCTestCore.framework",
    "XCTestSupport.framework",
    "XCUIAutomation.framework",
    "XCUnit.framework",
]

DYLIB_DEPS = [
    "libXCTestBundleInject.dylib",
    "libXCTestSwiftSupport.dylib",
]
