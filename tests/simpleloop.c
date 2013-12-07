/* simpleloop.c
 * A straightforward nested loop.  Tool should not be confused by inner
 * loop.
 */

#include <stdio.h>

int main(int argc, char *argv[]) {
    int i,j,sum;

    char c = 'a';

    for (j = 0; j < 20; ++j) {
        int k = j + 0x42;
        c += 1;
        sum--;
    }
}
