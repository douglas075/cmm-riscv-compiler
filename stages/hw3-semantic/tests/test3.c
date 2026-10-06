int test_3(int a, int b) {
	a = a + b;
	return a;
}

float test_3_2() {
	float b = 1;
	return b;
}
int main() {
	int a = 1;
	test_3(a, 2);
	a = test_3(a, 2);
	for (a = test_3(a, a); 1 == 1; a = test_3_2()) {
		;
	}
	return a;
}