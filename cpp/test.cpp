#include "util.hpp"
#include <iostream>

int main(){
    std::cout << util::wstr2str(L"test") << std::endl;
    std::cout << util::decorPath("C:\\ProgramData\\aviutl2\\Plugin\\python\\venv\\Scripts\\python.exe", "_mask") << std::endl;
}