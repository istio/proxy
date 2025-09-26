import Foundation
import Message_1
import Message_2

let message1 = Package1_Message1.with {
    $0.message = "Message1"
}
let message2 = Package2_Message2.with {
    $0.message1 = message1
}

print(message1)
print(message2)
