// ╔══════════════════════════════════════════════════════════════════════════════╗
// ║  Sample Code — Buffer Overflow                                              ║
// ║  This file has intentional buffer overflow for MemSentry to detect.         ║
// ╚══════════════════════════════════════════════════════════════════════════════╝
#include <iostream>
#include <cstring>

int main() {
    std::cout << "=== Sample: Buffer Overflow Demo ===\n\n";

    // Bug: Writing past the end of a heap buffer
    // MemSentry adds redzones (guard bytes 0xFD) around every allocation.
    // Writing past the buffer corrupts the redzone, which is detected on free.
    char* buf = new char[8];
    std::memset(buf, 'A', 8);       // OK — fills exactly 8 bytes
    buf[8] = 'X';                   // OVERFLOW — writes into the redzone!
    buf[9] = 'Y';                   // OVERFLOW — more corruption
    std::cout << "Wrote past buffer of 8 bytes → redzone corrupted\n";

    delete[] buf;  // MemSentry checks redzones here and reports overflow

    std::cout << "\nDone. MemSentry should report buffer overflow.\n";
    return 0;
}
