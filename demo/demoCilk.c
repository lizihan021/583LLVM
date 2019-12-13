#include "stdio.h"
#include "stdlib.h"
#include "math.h"
#include <cilk/cilk.h>

void highWorkLoad() {
    usleep(10000);
}

int main(){
    int n = 300;
    double *arr = (double *)malloc(n * sizeof(double));

    cilk_for(int i = 0; i < n; i++){
        if(i == (n/2)){
            arr[n/2] = arr[n/2-1] + arr[n/2-2];
        }
        else{
            arr[i] = pow(i, sin(cos(sin(cos(sin(i) * cos(i)))))) * sin(i);
            highWorkLoad();
        }
    }

    printf("%f\n\n", arr[n/2]);

    free(arr);
    return 0;
}

