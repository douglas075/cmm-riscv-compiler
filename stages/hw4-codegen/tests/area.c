float pi = 3.14;
float area(float r)
{
	return pi * r * r;
}
int main()
{
	float r, a;
	write("enter the radius:");
	r = read();
	a = area(r);
	write("the circle area is");
	write(a);
}