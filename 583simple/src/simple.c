#include <stdio.h>

#include "../../fp.h"

int global = -1;

void randomfcn() {
  int a = 5;
  a = a;
  int b = a;
  global = 5;
}

void pureFunc_PURE_(int *a) {
  global = *a;
}

void other_PURE_(int *a, int* b) {
  global = *a;
  global = *b;
}

int main()
{
  int val1 = 5;
  int val2 = 6;

  other_PURE_(&val1, &val2);
  other_PURE_(&val1, &val2);

  pureFunc_PURE_(&val1);
  pureFunc_PURE_(&val1);
  pureFunc_PURE_(&val1);
  pureFunc_PURE_(&val2);
  // try {
  //   randomfcn();
  // } catch(...) {}
  // int x = 0;
  // int y = x;
  // int x = 10;
  // int* xptr = &x;
  // int** xptrptr = &xptr;
  // int y = 15;
  // int* yptr = &y;
  // *xptrptr = yptr;
  // int arr[2];
  // arr[0] = 0;
  // global = 10;
  // int x = 10;
  // int y = x;
  // int* yPtr = &y;
  // int arr[8] = {0};
  // for (int i = 0; i < 8; ++i) {
  //   int z = *yPtr + 1;
  //   x += z;
  //   arr[i] = z + y;
  // }
  // randomfcn();
  // temp(&x);

  // int in[1000];
  // int i,j;
  // FILE* myfile;

  // for (i = 0; i < 1000; i++)
  // {
  //   in[i] = 0;
  // }

  // for (j = 100; j < 1000; j++)
  // {
  //  in[j]+= 10;
  // }


  // for (i = 0; i< 1000; i++)
  //   fprintf(stdout,"%d\n", in[i]);

  return 1;
}
