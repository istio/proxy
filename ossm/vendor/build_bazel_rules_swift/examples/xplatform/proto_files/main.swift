import Foundation
import Messages_1_2
import Messages_3

let message1 = ProtoFiles_Message1.with {
    $0.message = "Message1"
}
let message2 = ProtoFiles_Message2.with {
    $0.message1 = message1
}
let message3 = ProtoFiles_Message3.with {
    $0.message2 = message2
}

print(message1)
print(message2)
print(message3)
