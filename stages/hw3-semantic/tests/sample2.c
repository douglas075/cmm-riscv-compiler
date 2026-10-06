void test1(int var1, int var2) {
  a = 10; // symbol undeclared
}

void test2() {
  return 10; // return type mismatch
}

void test3() {
  int a;
  int a; // symbol redeclaration
}

void test4() {
  test1(10, 20, 30); // mismatch of the number of arguments
}

void test5() {
  int a[10];
  a[2.5] = 100;	// array subscript is not an integer
}

/*
 * Other possible errors: no main function, parameter type mismatch,
 * 	return an array, etc.
 */
