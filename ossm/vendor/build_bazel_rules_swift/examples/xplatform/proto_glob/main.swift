import Foundation
import SwiftProtobuf
import examples_xplatform_proto_glob_proto_glob_swift

let message1 = Package1_Message1.with {
    $0.query = "Message1"
}
let message2 = Package2_Message2.with {
    $0.query = "Message2"
}

print(message1)
print(message2)
