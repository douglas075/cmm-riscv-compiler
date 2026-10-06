int test_11(int a, int b) {
	a = a + b;
	return a;
}

float test_11_2() {
	float b = 1;
	return b;
}
int main() {
	int a = 1;
	test_11(a, 2);
	a = test_11(a, 2);
	for (a = test_11(a, a); 1 == 1; a = test_11_2()) {
		;
	}
	return a;
}