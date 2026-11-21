/* Author: Sherman White Jr., email: subscribeandnotifyme@gmail.com c. 2026 */
#include <ctype.h>
#include <string.h>

#include "utils.h"

// lower case a string in place

char *strlower(char *s) {

	char *p;

	for (p= s; *p; p++)
		*p= (char) tolower((unsigned char) *p);
	return s;
}

// convert a number written in base b to an int. An optional prefix
// ($ % & or 0x) is skipped. Returns -1 if the number is invalid.

int atobx(char *s, int b) {

	char *p= s;
	int a= 0;
	int d;

	if (b<2 || b>16)
		return -1;

	if (*p=='$' || *p=='%' || *p=='&')
		p++;
	else if (p[0]=='0' && (p[1]=='x' || p[1]=='X'))
		p+= 2;

	if (!*p)
		return -1;

	for (; *p; p++) {
		if (*p>='0' && *p<='9')
			d= *p-'0';
		else if (*p>='a' && *p<='f')
			d= *p-'a'+10;
		else if (*p>='A' && *p<='F')
			d= *p-'A'+10;
		else
			return -1;

		if (d>=b)
			return -1;

		if (a > (0x7fffffff-d)/b)	// would overflow
			return -1;

		a= a*b+d;
	}

	return a;
}
