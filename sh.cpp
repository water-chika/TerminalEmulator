#include <iostream>
#include <chrono>
#include <thread>

using namespace std::literals;

int main() {
    std::cout << "hello world!" << std::endl;
    while (true) {
        std::cout << std::chrono::steady_clock::now().time_since_epoch() << std::endl;
        std::this_thread::sleep_for(1s);
    }
    return 0;
}