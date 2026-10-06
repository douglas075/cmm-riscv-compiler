void test4(int a[10][10], int b[], int c[][10][10])
{
	a[1][2] = 3;
	b[1] = a[1][1];	
}

int main(){
	int d[10][10], e[10], f[10][10][10];
	test4(d, e, f);
}