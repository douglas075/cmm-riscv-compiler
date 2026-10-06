int sum_many(int a, int b, int c, int d, int e, int f, int g, int h, int i, int j) {
    return a+b+c+d+e+f+g+h+i+j;
}

int main() {
    int x = sum_many(1, 1, 1, 1, 1, 1, 1, 1, 10, 20);
    
    if (x == 38) {
        write("PASS");
    } else {
        write("FAIL");
    }
}