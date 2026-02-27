int main() {
    int a = 2 + 3 * (4 - 1) % 5;
    int b = !a || (a >= 5 && a < 10);
    int c = -a + +b;
    return a + b + c;
}
