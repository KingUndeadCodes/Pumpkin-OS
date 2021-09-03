#define NORMAL_FILE 		'0' // (ASCII NUL)
#define HARD_LINK		'1'
#define SYMBOLIC_LINK		'2'
#define CHARACTER_DEVICE	'3'
#define BLOCK_DEVICE		'4'
#define DIRECTORY		'5'
#define NAMED_PIPE		'6'

/*
    char filename[100];
    char mode[8];
    char uid[8];
    char gid[8];
    char size[12];
    char mtime[12];
    char chksum[8];
    char typeflag[1];
*/

typedef struct {
	char filename[100]; 		// File name
	char mode[8];   		// File mode
	char uid[8];    		// Owner's numeric user ID
	char gid[8];    		// Group's numeric user ID
	char size[12];  		// File size in bytes (octal base)
	char mtime[12];  		// Last modification time in numeric Unix time format (octal)
	char chksum[8];  		// Checksum for header record
	char typeflag[1];      		// Type Flag
} tar_t;
