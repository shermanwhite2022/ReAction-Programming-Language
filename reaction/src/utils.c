/* Author: Sherman White Jr., email: subscribeandnotifyme@gmail.com c. 2026 */
#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "asmtypes.h"

extern int _debug;
extern char *_garbage[];
extern int _gindex;

int charpos(char *s, char c) {
	int i;

	for (i=0; i<strlen(s); i++) 
		if (s[i] == tolower(c))
			return i;

	return (-1);
}

char *strip_leading_spaces(char *s) {
	int i;

	for (i=0; i<strlen(s); i++)
		if (!isspace(s[i]))
			return (&s[i]); 

	return (s);
}

char stripstr[256];

char *strip_quotes(char *s) {
	int size= strlen(s);

	memset(stripstr, 0, 256);

	if (ispunct(s[0])) {

		strncpy(stripstr, &s[1], size-2);

		if (_debug)
			printf("DBG: processing (%c) quotes = %s\n", s[0], stripstr);

		return (&stripstr[0]);
	}

	return (&s[0]);
}

char *strlower(char *s) {

	int i;
	char *out;

	out= strdup(s);
	// _garbage[_gindex++]= out;

	for (i=0; i<strlen(out); i++)
		out[i] = (char) tolower((int)out[i]);

	return out;
}

int atobx(char *s, int b) {

	int a;
	int i;
	int d;

	char *num;
	char ref[17]= "0123456789abcdef";

	if (b>16) 
		return -1;

	num= s;

	if (!charpos(s,'%') || !charpos(s, '$'))
		num= &s[1];

	if (!charpos(s,'0') && !charpos(&s[1], 'x')) 
		num= &s[2];

	num= strlower(num);

	a= 0;
	for (i=0; i<strlen(num); i++) {
		d= charpos(ref, num[i]);

		if (d>=b)
			return -1;

		a= a*b+d; 
	}

	return a;
}

int branch_calc(int pc, int oper) {
	int offset= oper-(pc+2);
	int result= (offset<0)? 256+offset: offset;

	if (_debug)
		printf("DBG: branch_calc: _pc= %x (%d) _oper= %x (%d) offset= %d result= %x (%d)\n", pc, pc, oper, oper, offset, result, result);

	return result;
}

char* replace_word(const char* s, const char* oldW, const char* newW)
{
	char* result;
	int i; 
	int cnt = 0;
	int newWlen = strlen(newW);
	int oldWlen = strlen(oldW);
  
    // Counting the number of times old word
    // occur in the string
	for (i = 0; s[i] != '\0'; i++) {
		if (strstr(&s[i], oldW) == &s[i]) {
			cnt++;
  
			// Jumping to index after the old word.
			i += oldWlen - 1;
		}
	}
  
    // Making new string of enough length

	result = (char*)malloc(i + cnt * (newWlen - oldWlen) + 1);
	// _garbage[_gindex++]= result;
  
	i = 0;
	while (*s) {
  		// compare the substring with the result
		if (strstr(s, oldW) == s) {
			strcpy(&result[i], newW);
			i += newWlen;
			s += oldWlen;
		}
		else
			result[i++] = *s++;
	}

	result[i] = '\0';
	return result;
}

int getCharOrd(char *s) {
	return (s[0]);
}
