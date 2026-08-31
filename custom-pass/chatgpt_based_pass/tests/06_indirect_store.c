int main(void) {
  int x;
  int y;
  int *p = &x;
  int **pp = &p;

  *pp = &y;
  return *p;
}
