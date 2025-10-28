void greet(int times) {
    int i = 0;
    while (i < times) {
        i = i + 1;
    }
    return;
}

int sum(int x, int y) {
    return x + y;
}

int main() {
    greet(3);
    int total = sum(1, 2);
    if (total != 3) {
        total = sum(total, 0);
    }
    return total;
}
