void test6_0(float a) {
	a = a + 1;
}

void test6_1(int a) {
	a = a + 1;
}

void test6_2() {
	typedef int myint;
	float a = 1;
	myint b = 0;
	test6_0(a); // correct
	test6_0(b); // warning: conversion int -> float
	test6_1(a); // warning: conversion float -> int
	test6_1("string"); // incorrect parameter type
}
