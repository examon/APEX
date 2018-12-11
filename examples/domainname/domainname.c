#include <err.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <limits.h>

char *__progname;
void usage(void);

int
main(int argc, char *argv[])
{
	int ch;
	char domainname[HOST_NAME_MAX+1];

	while ((ch = getopt(argc, argv, "")) != -1)
		switch (ch) {
		default:
			usage();
		}
	argc -= optind;
	argv += optind;

	if (argc > 1)
		usage();

	if (*argv) {
		if (setdomainname(*argv, strlen(*argv)))
			err(1, "setdomainname");
	} else {
		if (getdomainname(domainname, sizeof(domainname)))
			err(1, "getdomainname");
		(void)printf("%s\n", domainname);
	}
	return(0);
}


void usage(void)
{
	(void)fprintf(stderr, "usage: %s [name-of-domain]\n", __progname);
	exit(1);
}
