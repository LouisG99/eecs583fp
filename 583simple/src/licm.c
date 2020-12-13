#include <stdio.h>
#include <stdlib.h>
#include <time.h>

int main(int argc, char* argv[]) {
	double A[1000];
	int i;
	for(i = 0; i < 1000; i++){
		A[i] = i * 2.2398;
	}
	srand(time(NULL));
	int iters, aliasPeriod;
	sscanf(argv[1], "%d %*c %d", &iters, &aliasPeriod);
	fprintf(stderr, "Running %d times, with alias period %d\n", iters, aliasPeriod);

	const int j = 5;
	int k = j + 1;
	// %INITIAL = (A[j] * 3.1415926 + 3948.23891) / 27.5;
	for(i = 0; i < iters; i++) {
  		double temp = (A[j] * 3.1415926 + 3948.23891) / 27.5;
		// %temp = load(INITIAL);
		if(i % aliasPeriod == 0)
  			k = j;
		else if(i % aliasPeriod == 1)
  			k = j + 1;
		// IF ALIAS:
		//		%recalc = (A[k] * 3.1415926 + 3948.23891) / 27.5;
		// 		store(recalc, INITIAL)
		A[k] = temp;
	}

	return 0;
}
