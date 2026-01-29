int fib(int n) {
    if (n <= 1) {
        return n;
    } else {
        return fib(n - 1) + fib(n - 2);
    }
}

int main() {
    int i = 0;
    int sum = 0;
    while (i < 10) {
        if (i == 5) {
            i = i + 1;
            continue;
        }
        sum = sum + fib(i);
        if (sum > 100) break;
        i = i + 1;
    }
    return sum;
}
