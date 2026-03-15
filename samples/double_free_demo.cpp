// ╔══════════════════════════════════════════════════════════════════════════════╗
// ║  Sample Code — Double Free + Use After Free                                 ║
// ║  This file has intentional memory bugs for MemSentry to detect.             ║
// ╚══════════════════════════════════════════════════════════════════════════════╝
#include <iostream>
#include <cstring>

int main() {
    std::cout << "=== Sample: Double Free & Use-After-Free Demo ===\n\n";

    // Bug 1: Double free
    int* p = new int(42);
    std::cout << "Allocated int: " << *p << "\n";
    delete p;
    std::cout << "Freed once — OK\n";
    // delete p;  // Uncomment to trigger double-free (crashes without MemSentry)
    std::cout << "Second delete would be caught by MemSentry\n";

    // Bug 2: Use after free
    char* buf = new char[32];
    std::strcpy(buf, "Hello World");
    std::cout << "Buffer: " << buf << "\n";
    delete[] buf;
    // Accessing freed memory — MemSentry filled it with 0xDD
    std::cout << "After free, buf[0] = 0x"
              << std::hex << static_cast<int>(static_cast<unsigned char>(buf[0]))
              << std::dec << "\n";
    std::cout << "(Should be 0xDD if MemSentry is active)\n";

    // Bug 3: Allocation mismatch — new[] freed with delete
    int* arr = new int[10];
    for (int i = 0; i < 10; ++i) arr[i] = i;
    delete arr;  // Should be delete[] — mismatch!
    std::cout << "Used delete instead of delete[] — mismatch detected\n";

    std::cout << "\nDone.\n";
    return 0;
}
