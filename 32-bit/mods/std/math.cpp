#include "include/math.h"

// READ: https://github.com/Gwarek2/mathlib/blob/master/src/lib
// READ: https://wiki.osdev.org/User:Creature/Logarithms

double sqrt(double arg) {
    const double margin = 0.00001; // precision
    double s = arg;
    while ((s - arg / s) > margin) s = (s + arg / s) / 2;
    return s;
}

double fabs(double x) {
    return x < 0 ? -x : x;
}

int complexFloor(double x) {
    // Simply doing `return (int)x;` does not work.
    double xCopy = x;
    int xInt = 0;
    while (xCopy > 1) {
        xCopy--;
        xInt++;
    }
    return xInt;
}