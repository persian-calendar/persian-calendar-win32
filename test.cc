#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include "persian-calendar.h"

#define MAX_LINE_LENGTH 256

int main()
{
    FILE *file = fopen("test.txt", "r");
    if (!file)
    {
        perror("Error opening file");
        return EXIT_FAILURE;
    }

    char line[MAX_LINE_LENGTH];
    bool had_error = false;
    while (fgets(line, sizeof(line), file))
    {
        uint32_t gy, gm, gd, py, pm, pd;
        if (sscanf(line, "%d,%d,%d,%d,%d,%d", &gy, &gm, &gd, &py, &pm, &pd) == 6)
        {
            if (gregorian_to_days({gy, gm, gd}) != persian_to_days({py, pm, pd}))
            {
                printf("Error in %s", line);
                had_error = true;
            }
            unsigned days = gregorian_to_days({gy, gm, gd});
            {
                persian_date_t persian_date = days_to_persian(days);
                if (persian_date.year != py || persian_date.month != pm || persian_date.day != pd)
                {
                    printf("Error in %s", line);
                    had_error = true;
                }
            }
            {
                unsigned days = persian_to_days({py, pm, pd});
                gregorian_date_t gregorian_date = days_to_gregorian(days);
                if (gregorian_date.year != gy || gregorian_date.month != gm || gregorian_date.day != gd)
                {
                    printf("Error in %s", line);
                    had_error = true;
                }
            }
        }
        else fprintf(stderr, "Invalid line format: %s", line);
    }

    fclose(file);
    return had_error ? EXIT_FAILURE : EXIT_SUCCESS;
}