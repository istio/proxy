#include <fstream>
#include <iostream>
#include <string>

extern int a, b;

// A very roundabout hello world.
int main(int argc, char* argv[]) {
  std::string runfiles(argv[0]);
  runfiles.append(".runfiles");
  std::string hello(runfiles + "/rules_pkg/tests/testdata/hello.txt");
  std::fstream fs;
  fs.open(hello, std::iostream::in);
  char tmp[1000];
  fs.read(tmp, sizeof(tmp));
  fs.close();
  std::cout << tmp;

  return (a + b > 0) ? 0 : 1;
}
