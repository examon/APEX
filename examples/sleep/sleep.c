#include <ctype.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
#include <err.h>

extern char *__progname;
void usage(void);
void alarmh(int);

int
main(int argc, char *argv[])
{
    int ch;
    time_t secs = 0, t;
    char *cp;
    int nsecs = 0;
    struct timespec rqtp;
    int i;

    signal(SIGALRM, alarmh);

    while ((ch = getopt(argc, argv, "")) != -1)
        switch(ch) {
        default:
            usage();
    }
    argc -= optind;
    argv += optind;

    if (argc != 1)
        usage();

    cp = *argv;
    while ((*cp != '\0') && (*cp != '.')) {
        if (!isdigit((unsigned char)*cp))
            errx(1, "seconds is invalid: %s", *argv);
        t = (secs * 10) + (*cp++ - '0');
        if (t / 10 != secs)	/* oflow */
            errx(1, "seconds is too large: %s", *argv);
        secs = t;
    }

    /* Handle fractions of a second */
    if (*cp == '.') {
        cp++;
        for (i = 100000000; i > 0; i /= 10) {
            if (*cp == '\0')
                break;
            if (!isdigit((unsigned char)*cp))
                errx(1, "seconds is invalid: %s", *argv);
            nsecs += (*cp++ - '0') * i;
        }

        while (*cp != '\0') {
            if (!isdigit((unsigned char)*cp++))
                errx(1, "seconds is invalid: %s", *argv);
        }
    }

    while (secs > 0 || nsecs > 0) {
        if (secs > 100000000) {
            rqtp.tv_sec = 100000000;
            rqtp.tv_nsec = 0;
        } else {
            rqtp.tv_sec = secs;
            rqtp.tv_nsec = nsecs;
        }
        if (nanosleep(&rqtp, NULL))
            err(1, NULL);
        secs -= rqtp.tv_sec;
        nsecs -= rqtp.tv_nsec;
    }
    return (0);
}

void
usage(void)
{
    (void)fprintf(stderr, "usage: %s seconds\n", __progname);
    exit(1);
}

void
alarmh(int signo)
{
    _exit(0);
}
