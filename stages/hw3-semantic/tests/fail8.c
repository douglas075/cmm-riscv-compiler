int a;
int test_10(float a) {
	int b[10];
	// type of a should be float 
	// and illegal for subscription here
	b[a] = 0;	
}
int main(){}