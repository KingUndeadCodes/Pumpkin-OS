#include "include/math.h"

double sqrt(double arg) {
    const double margin = 0.00001; // precision
    double s = arg;
    while ((s - arg / s) > margin) s = (s + arg / s) / 2;
    return s;
}