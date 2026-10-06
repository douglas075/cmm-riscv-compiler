/* multi decl in one line */
/* with initial value being some expr */
int a, b = 3, c = 1 + 3;
/* array version */ 
float d[3], e[1+3], f[1][2][3][4];

typedef int myint;
myint g;

int main()
{
	
	{
		{
			{
				{
					{
						;;
					}
				}
			}
		}
	}
	if (a = b = c) {
		if (a != a || b / b) {
		/* empty stmt */
		}
	} 
	else {
		if (1) 2; else 3;
	}
	
	/* nest statement*/
	
	if (1) 
		while (b)
			for (c = 0; c < d[a]; c = c+1)
				if (c)
					for (;;)
						while (1)
							if (0) ; else ;

	int bp;
						

}

int f(myint a[], myint b[][1000])
{
	
}
/* can be compiled by gcc */
int empty_func()
{
	{}{}{}
	{
		{
			{

			}
		}
	}
}