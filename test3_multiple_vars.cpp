#include <iostream>
#include <string>

void calculate_geometry() {
    double radius = 5.0;
    double area = 3.14159 * radius * radius;
    double perimeter_unused = 2 * 3.14159 * radius;
    
    std::cout << "Area: " << area << std::endl;
}

void print_greeting() {
    std::string greeting = "Hello";
    std::string unused_msg = "Never shown";
    
    std::cout << greeting << " World!" << std::endl;
}

int main() {
    int count = 5;
    int dead_count = 0;
    
    for (int i = 0; i < count; ++i) {
        calculate_geometry();
    }
    
    print_greeting();
    return 0;
}
