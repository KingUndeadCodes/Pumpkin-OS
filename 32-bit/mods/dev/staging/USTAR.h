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
	char File_Name[100]; 		// File name
	char File_Mode[8];   		// File mode
	char File_UID[8];    		// Owner's numeric user ID
	char File_GID[8];    		// Group's numeric user ID
	char File_Size[12];  		// File size in bytes (octal base)
	char File_Time[12];  		// Last modification time in numeric Unix time format (octal)
	char File_Check[8];  		// Checksum for header record
	char File_Flag;      		// Type Flag
	char File_LinkName[100];	// Name of linked file
} tar_t;
