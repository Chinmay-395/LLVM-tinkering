int main(int argc, char **argv) {
  int x;
  int y;
  int *p;

  if (argc > 10)
    p = &x;
  else
    p = &y;

  return *p;
}
