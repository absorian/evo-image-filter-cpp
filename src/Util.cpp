#include <random>

std::random_device g_random_device;
std::mt19937 g_mt19937(g_random_device());

double drand(double a, double b) {
    std::uniform_real_distribution distribution(a, b);
    return distribution(g_mt19937);
}

int lrand(int a, int b) {
    std::uniform_int_distribution distribution(a, b - 1);
    return distribution(g_mt19937);
}