#include <stdio.h>
#include "kblayout.c"

int main() {
	printf("Layout:\n");
	printf("%s\n", detect_layout());
	return 0;
}

