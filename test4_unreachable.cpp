#include <iostream>

int calculate() {
    std::cout << "Calculating..." << std::endl;
    return 100;
    std::cout << "This line is unreachable after return!" << std::endl;
    int dead_code = 999;
}

void process_loop() {
    for (int i = 0; i < 5; ++i) {
        std::cout << "Iter: " << i << std::endl;
        break;
        std::cout << "This line is unreachable after break!" << std::endl;
    }
}

int main() {
    int res = calculate();
    process_loop();
    std::cout << "Result: " << res << std::endl;
    return 0;
}
