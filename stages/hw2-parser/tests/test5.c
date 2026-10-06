int main() {
    int i, sum;
    sum = 0;
    for (i = 1; i <= 5; i = i + 1) {
        sum = sum + i;
    }
    write("Sum is:");
    write(sum);
}
