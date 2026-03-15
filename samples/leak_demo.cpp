// ╔══════════════════════════════════════════════════════════════════════════════╗
// ║  Sample Code — Memory Leak                                                  ║
// ║  This file has intentional memory bugs for MemSentry to detect.             ║
// ╚══════════════════════════════════════════════════════════════════════════════╝
#include <iostream>
#include <cstring>

int main() {
    std::cout << "=== Sample: Memory Leak Demo ===\n\n";

    // Bug 1: Memory leak — allocated but never freed
    int* numbers = new int[100];
    for (int i = 0; i < 100; ++i) numbers[i] = i * i;
    std::cout << "Allocated 100 ints (never freed — LEAK)\n";

    // Bug 2: Another leak
    char* name = new char[64];
    std::strcpy(name, "MemSentry");
    std::cout << "Allocated string '" << name << "' (never freed — LEAK)\n";

    // This one is properly freed — no bug
    double* temp = new double(3.14);
    std::cout << "Allocated double: " << *temp << " (will free)\n";
    delete temp;

    std::cout << "\nDone. MemSentry should report 2 leaks.\n";
    return 0;
}
