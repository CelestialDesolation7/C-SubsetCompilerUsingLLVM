int main() {
    int i = 0;
    int j = 0;
    while (i < 3) {
        j = 0;
        while (j < 3) {
            if (i == j) {
                j = j + 1;
                continue;
            }
            int prod = i * j;
            if (prod > 2) {
                break;
            }
            j = j + 1;
        }
        i = i + 1;
    }
    return i + j;
}
