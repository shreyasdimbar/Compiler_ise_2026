#include <iostream>

int side_effect_func() {
    std::cout << "[Side Effect] Function executed!" << std::endl;
    return 42;
}

int main() {
    int pure_unused = 100;
    int side_effect_var = side_effect_func();
    int used_var = 10;
    
    std::cout << "Used var: " << used_var << std::endl;
    return 0;
}
