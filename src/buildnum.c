#include "buildnum.h"

#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

const char* editor_build_date = __DATE__;
const char* editor_build_time = __TIME__;

// static char *date = "Sep 13 2020";
static const char* date = __DATE__;

static const char* mon[12] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun",
                              "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
static char mond[12] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};

static bool computed = false;
static int build_number;

static void computeBuildNumber(void) {
    int m = 0;
    int d = 0;
    int y = 0;

    for (m = 0; m < 11; m++) {
        if (strncmp(&date[0], mon[m], 3) == 0)
            break;
        d += mond[m];
    }

    d += atoi(&date[4]) - 1;

    y = atoi(&date[7]) - 1900;

    build_number = d + (int)((y - 1) * 365.25);

    if (((y % 4) == 0) && m > 1) {
        build_number += 1;
    }

    build_number -= 43720;  // Sep 13 2020 (holoEN Myth Date)
}

// Returns days since Sep 13 2020
int editorGetBuildNumber(void) {
    if (!computed) {
        computeBuildNumber();
    }
    return build_number;
}
