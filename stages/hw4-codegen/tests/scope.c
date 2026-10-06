int x;
void write_x()
{
  write(x);
}
int main() {
  int x;
  write_x();
  x = 1;
  {
    int x;
    x = 2;
    write(x);
  }
  write(x);
}
