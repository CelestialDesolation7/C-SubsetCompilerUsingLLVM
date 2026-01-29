int main() {
    {
        int a = 2;
        {
            a = a * 3;
        }
    }
    return 0;
}
