int helper(int n);
int main() {
    volatile int n = 7;
    n += 1;
    return helper(n);
}
