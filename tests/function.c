/* sequential.c
 * Control test program with no jumps
 */

#include <stdio.h>

void function(int i) {
    int j = i;
    printf("%d ", j);
}

int main(int argc, char *argv[]) {
    int i;

    for (i = 0; i < 10; ++i) {
        int j;
        function(i);
    }

    for (i = 0; i < 100; ++i) {
        function(i);
    }
}
