#include <iostream>
using namespace std;
void unused_dead_function() {
 cout << "I am never called!" << endl;
}

void used_function() {
    cout << "I am called!" << endl;
}

int main() {
    used_function();
    
    int x = 5;
    x = x;
    
    int y = 1;
    y = 2;
    
    ;
    {}
    
  cout << "y final: " << y << endl;
    return 0;
}
