#include <iostream>

void process() {
    int unused = 50;
    double pi_unused = 3.14159;
    bool flag = false;
    
    int active = 42;
    std::cout << "Active value: " << active << std::endl;
}

int main() {
    process();
    return 0;
}
