#include <windows.h>
#include <iostream>

int main() {
    std::cout << "Dummy Hogwarts Legacy running..." << std::endl;
    // Allocate some memory so it shows up with a decent size
    void* mem = malloc(150 * 1024 * 1024);
    if (mem) memset(mem, 0, 150 * 1024 * 1024);
    
    Sleep(60000); // run for 60 seconds
    return 0;
}
