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

double pow(double a, double b) {
    double c = 1;
    for (int i=0; i<b; i++)
        c *= a;
    return c;
}

double fact(double x) {
    double ret = 1;
    for (int i=1; i<=x; i++) 
        ret *= i;
    return ret;
}

double sin(double x) {
    double y = x;
    double s = -1;
    for (int i=3; i<=100; i+=2) {
        y+=s*(pow(x,i)/fact(i));
        s *= -1;
    }  
    return y;
}

double cos(double x) {
    double y = 1;
    double s = -1;
    for (int i=2; i<=100; i+=2) {
        y+=s*(pow(x,i)/fact(i));
        s *= -1;
    }  
    return y;
}

double tan(double x) {
    return (sin(x) / cos(x));  
}


/*
// AI GENERATED
double asin(double x) {
    double y = x;
    double s = 1;
    for (int i=3; i<=100; i+=2) {
        y+=s*(pow(x,i)/(i*(i-1)));
        s *= -1;
    }  
    return y;
}

// AI GENERATED
double acos(double x) {
    return (M_PI/2)-asin(x);
}

// AI GENERATED
double atan(double x) {
    double y = x;
    double s = -1;
    for (int i=3; i<=100; i+=2) {
        y+=s*(pow(x,i)/i);
        s *= -1;
    }  
    return y;
}
*/