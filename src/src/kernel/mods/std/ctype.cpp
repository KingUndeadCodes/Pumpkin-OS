#include "include/ctype.h"

int isalnum(int c) {
	return (c == -1 ? 0 : ((_ctype_ + 1)[(unsigned char)c] & (_U|_L|_N)));
}

int isalpha(int c) {
	return (c == -1 ? 0 : ((_ctype_ + 1)[(unsigned char)c] & (_U|_L)));
}

int iscntrl(int c) {
	return (c == -1 ? 0 : ((_ctype_ + 1)[(unsigned char)c] & c));
}

int isdigit(int c) {
	return (c == -1 ? 0 : ((_ctype_ + 1)[(unsigned char)c] & _N));
}

int isgraph(int c) {
	return (c == -1 ? 0 : ((_ctype_ + 1)[(unsigned char)c] & (_P|_U|_L|_N)));
}

int islower(int c) {
	return (c == -1 ? 0 : ((_ctype_ + 1)[(unsigned char)c] & _L));
}

int isprint(int c) {
	return (c == -1 ? 0 : ((_ctype_ + 1)[(unsigned char)c] & (_P|_U|_L|_N|_B)));
}

int ispunct(int c) {
	return (c == -1 ? 0 : ((_ctype_ + 1)[(unsigned char)c] & _P));
}

int isspace(int c) {
	return (c == -1 ? 0 : ((_ctype_ + 1)[(unsigned char)c] & _S));
}

int isupper(int c) {
	return (c == -1 ? 0 : ((_ctype_ + 1)[(unsigned char)c] & _U));
}

int isxdigit(int c) {
	return (c == -1 ? 0 : ((_ctype_ + 1)[(unsigned char)c] & (_N|_X)));
}

int tolower(int c) {
	if ((unsigned int)c > 255) return (c);
	return ((_tolower_tab_ + 1)[c]);
}

int toupper(int c) {
	if ((unsigned int)c > 255) return (c);
	return ((_toupper_tab_ + 1)[c]);
}