int compute(int x, int y) {
    int res = 0;
    {
        int temp = x * y;
        res = temp / (x + 1);
    }
    return res;
}

int main() {
    int p = 10, q = 5;
    int r = compute(p, q);
    if (r >= 10) {
        r = r - 10;
    } else if (r > 0) {
        r = r + 1;
    } else {
        r = !p;
    }
    return r;
}
