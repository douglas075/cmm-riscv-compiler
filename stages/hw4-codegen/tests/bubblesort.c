int main()
{
	int a[10];
	int i, j, n, tmp;
	n = 10;
	write("enter 10 integers");
	for (i = 0; i < n; i = i + 1)
		a[i] = read();

	for (i = n - 1; i > 0; i = i - 1)
	{
		for (j = 0; j <= i - 1; j = j + 1)
		{
			if(a[j] > a[j + 1])
			{
				tmp = a[j];
				a[j] = a[j + 1];
				a[j + 1] = tmp;
			}
		}
	}

	for (i = 0; i < n; i = i + 1)
		write(a[i]);
}