#include <stdio.h>
#include "3270ds.h"

main()
{
	int i;

	printf("n\tIntensity\tSelectable\tType\t\tModified\n");

	for (i = 0xc0; i < 0xd0; i++)
		attributes(i);
	for (i = 0xe0; i < 0xf0; i++)
		attributes(i);
	exit(0);
}

attributes(i)
int i;
{
	printf("%02X", i);
	if (FA_IS_ZERO(i))
		printf("\tzero\t");
	else if (FA_IS_HIGH(i))
		printf("\thigh\t");
	else
		printf("\tnormal\t");
	if (FA_IS_SELECTABLE(i))
		printf("\tsel\t");
	else
		printf("\t-\t");
	if (FA_IS_SKIP(i))
		printf("\tauto-skip");
	else {
		if (FA_IS_NUMERIC(i))
			printf("\tnumeric\t");
		else if (FA_IS_PROTECTED(i))
			printf("\tprotected");
		else
			printf("\t-\t");
	}
	if (FA_IS_MODIFIED(i))
		printf("\tmod");
	else
		printf("\t-");
	printf("\n");
}
