#include <iostream>

extern "C" {

void bar() {
  std::cout << "bar" << std::endl;
}

}
