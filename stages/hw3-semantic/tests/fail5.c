void test5() {
  int a[10];
  float b = 2.5;
  a[b] = 100;
  a[2.5] = 100;	// array subscript is not an integer
}