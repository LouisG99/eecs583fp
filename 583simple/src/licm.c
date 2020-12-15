#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include "../../fp.h"

void initArr(double* A, int size) {
	int i;
	for(i = 0; i < size; i++){
		A[i] = i * 2.2398;
	}
}

int main(int argc, char* argv[]) {
	if (argc != 2) {
		printf("bad bad no\n");
		return 1;
	}
	srand(time(NULL));
	int iters, aliasPeriod;
	sscanf(argv[1], "%d %*c %d", &iters, &aliasPeriod);
	fprintf(stderr, "Running %d times, with alias period %d\n", iters, aliasPeriod);

	double A[1000];
	initArr(A, 1000);
	

	const int j = 5;
	int k = j + 1;
	for(int i = 0; i < iters; i++) {
  	double temp = (A[j] * 3.1415926 + 3948.23891) / 27.5;
		if(i % aliasPeriod == 0)
  			k = j;
		else if(i % aliasPeriod == 1)
  			k = j + 1;
		A[k] = temp;
	}

	return 0;
}

// %INITIAL = (A[j] * 3.1415926 + 3948.23891) / 27.5;
// %temp = load(INITIAL);
// IF ALIAS:
//		%recalc = (A[k] * 3.1415926 + 3948.23891) / 27.5;
// 		store(recalc, INITIAL)
