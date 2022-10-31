#ifndef _CTYPE_H_
#define _CTYPE_H_

// Most code taken from: https://github.com/openbsd/src/blob/master/include/ctype.h

#define	_U 0x01
#define	_L 0x02
#define	_N 0x04
#define	_S 0x08
#define	_P 0x10
#define	_C 0x20
#define	_X 0x40
#define	_B 0x80

extern const char *_ctype_;
extern const short *_tolower_tab_;
extern const short *_toupper_tab_;

int isalnum(int c);
int iscntrl(int c);
int isgraph(int c);
int islower(int c);
int isprint(int c);
int ispunct(int c);
int isupper(int c);
int isxdigit(int c);
int isalpha(int c);
int isdigit(int c);
int isspace(int c);
int toupper(int c);

#endif