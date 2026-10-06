int main() {
    int a[3], b[2][4];
    int i, j, s;
    s = 0;
    a[0] = 1; a[1] = 2; a[2] = 3;
    for (i = 0; i < 2; i = i + 1) {
        for (j = 0; j < 4; j = j + 1) {
            b[i][j] = i + j;
            s = s + b[i][j];
        }
    }
    write(a[0] + a[1] + a[2] + s);
}
