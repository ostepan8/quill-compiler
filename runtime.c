#include <stdio.h>

void print_double(double value) {
    // For integer-like values, print without decimal places
    if (value == (long long)value) {
        printf("%.0f\n", value);
    } else {
        printf("%.6f\n", value);
    }
}