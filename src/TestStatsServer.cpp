#include <stdio.h>
#include "StatsServer.h"

#define NUMOF(x) (sizeof(x) / sizeof(*x))

int main(int argc, char **argv)
{
    int stats_example[] = {25, 24, 22, 21, 56, 56, 54, 53, 51, 45, 42, 39, 34, 30, 24, 15, 8, 4, 1, 0};
    StatsServer serv(2720);

    serv.registerInput("foo");
    while (true) {
        for (int i = 0; i < NUMOF(stats_example); i++) {
            usleep(400000);
            serv.notifyBuffer("foo", stats_example[i]);
            fprintf(stderr, "give %d\n", stats_example[i]);

            if (stats_example[i] == 0) {
                serv.notifyUnderrun("foo");
            }
        }
    }

    return 0;
}

