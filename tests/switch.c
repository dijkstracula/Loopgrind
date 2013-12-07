/* simpleloop.c
 * A straightforward nested loop.  Tool should not be confused by inner
 * loop.
 */

#include <stdio.h>

int main(int argc, char *argv[]) {
    int i,j;

    for (i = 0; i < 10; ++i) {
        j++;
    }

    for (i = 0; i < 20; ++i) {
        j--;
    }

    for (i = 0; i < 100; ++i) {
        switch(i % 7)
        {
            case 0:
                j++;
                break;
            case 1:
                j--;
                break;
            case 2:
                j *= 2;
                break;
            case 3:
                j -= i;
                break;
            case 4:
                j--;
                break;
            case 5:
                j *= 2;
                break;
            case 6:
                j -= i;
                break;

        }
    }
    i = 0;
}
