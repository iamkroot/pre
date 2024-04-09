#include <stdio.h>

int main()
{
    int x = 1;
    int y = x + 1;
    int a = y + 1;
    int b = a + 1;

    if (a > 0) {
        x = 3;
    }
    else {
        x = a + b;
    }

    y = a + b;
    return 0;
}