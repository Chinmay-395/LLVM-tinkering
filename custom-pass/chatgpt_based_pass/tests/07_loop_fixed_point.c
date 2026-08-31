int main(int argc, char **argv) {
  int x;
  int y;
  int *p = &x;

  while (argc-- > 0)
    p = &y;

  return *p;
}
