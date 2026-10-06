int main() {
    int i, s, stop;
    i = 1; s = 0; stop = 0;
    for ( ; stop == 0 ; ) {
        s = s + i;
        i = i + 1;
        if (i > 5) { stop = 1; }
    }
    write(s);
}
