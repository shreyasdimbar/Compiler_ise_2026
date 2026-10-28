#include <iostream>

int main() {
    int folded_sum = 10 + 20;
    int folded_mult = 5 * 4;
    
    const int FACTOR = 2;
    int propagated_val = 50 * FACTOR;
    
    std::cout << "Sum: " << folded_sum << ", Mult: " << folded_mult << ", Prop: " << propagated_val << std::endl;
    return 0;
}
