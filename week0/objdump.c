#include <stdio.h>


void saxpy(float * y, const float * x, float a, int n) 
{
    for (int i = 0; i < n; i++)
        y[i] = a * x[i] + y[i];
}