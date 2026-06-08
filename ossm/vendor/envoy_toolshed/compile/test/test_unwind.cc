#include <iostream>
#include <stdexcept>

int main() {
    try {
        throw std::runtime_error("testing unwinding");
    } catch (const std::exception& e) {
        std::cout << "caught: " << e.what() << std::endl;
    }
    return 0;
}
