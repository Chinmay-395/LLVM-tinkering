int main(int argc, char **argv) {
  int x;
  int y;
  int z;
  int *p = &x;
  int *q = &y;
  int **r;

  if (argc > 1)
    r = &p;
  else
    r = &q;

  *r = &z;
  return 0;
}
