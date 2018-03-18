
// INCLUDES
#include <stdio.h>
#include <fcntl.h>
#include <dirent.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>

// DEFINES
#define VALID_PATH 1
#define INVALID_PATH 0
#define VALID_DIR_PATH 2
#define MAX_PATH_LEN 256
#define BUFF_SIZE 100000

// SECTION HEADER
typedef struct s_header{
	uint8_t		sect_type;
	uint32_t 	sect_size;
	uint32_t 	sect_offset;
	char 			sect_name[21];
} sect_header;

// MAIN HEADER
typedef struct hheader{
	char 	magic[5];
	uint16_t 	h_size;
	uint16_t	version;
	uint8_t 	nb_sections;
	sect_header sh[255];
} header;

// CHECKS WHETHER THE GIVEN PATH IS A VALID FILE / DIRECTORY
int valid(char *path){
	struct stat fileMetadata;

	if (stat(path, &fileMetadata) < 0){
		return INVALID_PATH;
	}
	else if (S_ISDIR(fileMetadata.st_mode)){
		return VALID_DIR_PATH;
	}
	return VALID_PATH;
}

// READS FROM A SF INTO THE HEADER STRUCTURE
header read_SF(char *path){
	int fd;
	header head;

	fd = open(path, O_RDONLY);
	if (fd >  0){																			// read header fields
		read(fd, head.magic, 4);
		head.magic[4] = 0;
		read(fd, &(head.h_size), 2);
		read(fd, &(head.version), 2);
		read(fd, &(head.nb_sections), 1);

		for (int i = 0; i < head.nb_sections; i++){			// read all file section fields
			read(fd, head.sh[i].sect_name, 20);
			head.sh[i].sect_name[20] = 0;
			read(fd, &(head.sh[i].sect_type), 1);
			read(fd, &(head.sh[i].sect_offset), 4);
			read(fd, &(head.sh[i].sect_size), 4);
		}

		close(fd);
	}

	return head;
}

// CHECKS WHETHER THE SF FORMAT COMPLIES WITH THE GIVEN CONDITIONS
int valid_SF(header h, int print){
	if (strcmp(h.magic, "o81P")){																	// check magic
		if (print){
			printf("ERROR\nwrong magic\n");
		}
		return 1;
	}

	if ((h.version < 18) || (h.version > 112)){										// check version
		if (print){
			printf("ERROR\nwrong version\n");
		}
		return 2;
	}

	if ((h.nb_sections < 3) || (h.nb_sections > 17)){							// check nb of sections
		if (print){
			printf("ERROR\nwrong sect_nr\n");
		}
		return 3;
	}

	for (int i = 0; i < h.nb_sections; i++){											// check all sections' type
		if ((h.sh[i].sect_type < 56) || (h.sh[i].sect_type > 82)){
			if (print){
				printf("ERROR\nwrong sect_types\n");
			}
			return 4;
		}
	}
	return 0;
}

// CHECKS WHETHER THE LISTING OPTIONS OF THE LIST COMMAND ARE MET
int isPrintable(char *path, int size_filter, int name_filter, char *prefix){
	int name_ok = 1;
	int size_ok = 1;
	struct stat fileMetadata;

	if (size_filter == -1 && name_filter == -1){		// if no filter is needed, the file is good to go
		return 1;
	}

	stat(path, &fileMetadata);											// fetch file details

	// check size
	if (/*(S_ISDIR(fileMetadata.st_mode) == 0) && */(fileMetadata.st_size <= size_filter) && (size_filter > -1)){
		size_ok = 0;
	}

	// check name
	if ((strstr(strrchr(path, '/') + 1, prefix) != strrchr(path, '/') + 1) && (name_filter > -1)){
		name_ok = 0;
	}

	// return result
	return (name_ok && size_ok);
}

// CHECKS WHETHER A SF COMPLIES WITH THE FINDALL COMMAND CONDITIONS
int is_findable(header h){
	int sections = 0;

	for(int i = 0; i < h.nb_sections; i++){
		if(h.sh[i].sect_type == 56){
			sections++;
		}
	}
	// file has to have at least 5 sections of type 56
	if (sections > 4){
		return 1;
	}
	return 0;
}

// LISING PROCEDURE
void list(int rec, int size_filter, int name_filter, char *prefix, char *path, int first_call){

	DIR* dir;
	struct dirent *dirEntry;

	dir = opendir(path);
	if (dir == 0) {
		return;
	}

	// if dir has successfully been opened and it is the first recursive call => success
	if (first_call){
		printf("SUCCESS\n");
	}

	while ((dirEntry = readdir(dir)) != 0) {
		if(strcmp(dirEntry->d_name, ".") && strcmp(dirEntry->d_name, "..")){  // . and .. directories are undesired
			char full_path[100];
			strcpy(full_path, path);
			strcat(full_path, "/");
			strcat(full_path, dirEntry->d_name);																// construct the full path of the file

			if (isPrintable(full_path, size_filter, name_filter, prefix)){
				printf("%s\n", full_path);
			}

			if (rec){																														// if recursive parameter is met, call list() on the current path
				list(rec, size_filter, name_filter, prefix, full_path, 0);
			}
		}
	}

	closedir(dir);
}

// PARSING PROCEDURE
void parser(char *path){
	header head = read_SF(path);

	if(valid_SF(head, 1) == 0){											// if SF is valid, print as required
		printf("SUCCESS\n");
		printf("version=%d\nnr_sections=%d\n", head.version, head.nb_sections);

		for (int i = 0; i < head.nb_sections; i++){
			printf("section%d: %s %d %d\n", i + 1, head.sh[i].sect_name, head.sh[i].sect_type, head.sh[i].sect_size);
		}
	}
}

// EXTRACTING PROCEDURE
void extract(header h, char *path, int section, int line){
	int fd;
	char *buf;
	int endln = 0;
	uint32_t i = 0;

	if ((fd = open(path, O_RDONLY)) >  0){
		printf("SUCCESS\n");

		// set the cursor at the offset of the specified section number
		lseek(fd, h.sh[section - 1].sect_offset, SEEK_SET);
		buf = (char*)malloc(h.sh[section - 1].sect_size + 1);		// allocate the buffer with the section size
		read(fd, buf, h.sh[section - 1].sect_size);							// read the section into the buffer

		while ((endln < line - 1)  && (i < h.sh[section - 1].sect_size)){
			if(buf[i] == '\n'){
				endln++;																						// count the lines
			}
			i++;
		}

		while ((buf[i] != '\n') && (i < h.sh[section - 1].sect_size)){
			printf("%c", buf[i]);																	// print the desired line
			i++;
		}
		printf("\n");

		free(buf);
		close(fd);
	}
	else{
		printf("ERROR\ninvalid file\n");
	}
}

// FINDING PROCEDURE
void findall(char *path, int first_call){
	DIR* dir;
	struct dirent *dirEntry;

	dir = opendir(path);
	if (dir == 0) {
		return;
	}

	if (first_call){
		printf("SUCCESS\n");
	}

	while ((dirEntry = readdir(dir)) != 0) {
		if(strcmp(dirEntry->d_name, ".") && strcmp(dirEntry->d_name, "..")){
			char full_path[100];
			strcpy(full_path, path);
			strcat(full_path, "/");
			strcat(full_path, dirEntry->d_name);

			header head = read_SF(full_path);

			if ((valid_SF(head, 0) == 0) && (is_findable(head))){
				printf("%s\n", full_path);
			}
			findall(full_path, 0);
		}
	}

	closedir(dir);
}

// SELECTING COMMAND NAME PROCEDURE
void select_cmd(int argc, char **argv){
	int i;
	int v = 0;
	int l = 0;
	int p = 0;
	int e = 0;
	int f = 0;

	// check which commands have been specified
	for (i = 1; i < argc; i++){
		if (strcmp(argv[i], "variant") == 0){
			v = i;
		}
		else if (strcmp(argv[i], "list") == 0){
			l = i;
		}
		else if (strcmp(argv[i], "parse") == 0){
			p = i;
		}
		else if (strcmp(argv[i], "extract") == 0){
			e = i;
		}
		else if (strcmp(argv[i], "findall") == 0){
			f = i;
		}
	}

	// DISPLAY VARIANT COMMAND
	if(strcmp(argv[v], "variant") == 0){
		if (l || p || e || f){
			printf("ERROR\nplease execute only one command at once\n");
			return;
		}
		printf("76911\n");
		return;
	}

	// LIST COMMAND
	else if(strcmp(argv[l], "list") == 0){
		int arg_nb;
		int path_arg;
		char prefix[50];
		int recursive = 0;
		int size_greater = -1;
		int name_starts_with = -1;

		if (v || p || e || f){
			printf("ERROR\nplease execute only one command at once\n");
			return;
		}

		// fetching the command options
		for (arg_nb = 2; arg_nb < argc; arg_nb++){
			if(strcmp(argv[arg_nb], "recursive") == 0){
				recursive++;
			}
			if(strncmp(argv[arg_nb], "name_starts_with=", 17) == 0){
				name_starts_with++;
				strcpy(prefix, argv[arg_nb] + 17);
			}
			if(strncmp(argv[arg_nb], "size_greater=", 13) == 0){
				size_greater = atoi(argv[arg_nb] + 13);
			}
			if(strncmp(argv[arg_nb], "path=", 5) == 0){
				path_arg = arg_nb;
			}
		}

		// calling the listing procedure
		if(argc > 2 && path_arg){
			if (valid(argv[path_arg] + 5) == VALID_DIR_PATH){
				list(recursive, size_greater, name_starts_with, prefix, argv[path_arg] + 5, 1);
			}
			else {
				printf("ERROR\ninvalid directory path\n");
				return;
			}
		}
		else if(argc < 3 || path_arg == 0){
			printf("ERROR\nplease provide a path\n");     // lack of path parameter
			return;
		}
	}

	// PARSE COMMAND
	else if(strcmp(argv[p], "parse") == 0){

		if (v || l || e || f){
			printf("ERROR\nplease execute only one command at once\n");
			return;
		}

		if(argc > 2 && (strncmp(argv[2], "path=", 5) == 0)){
			if (valid(argv[2] + 5) == VALID_PATH){
				parser(argv[2] + 5);
			}
			else{
				printf("ERROR\ninvalid path\n");
				return;
			}
		}
		else {
			printf("ERROR\nplease provide a path\n");			// lack of path parameter
			return;
		}
	}

	// EXTRACT COMMAND
	else if(strcmp(argv[e], "extract") == 0){
		int arg_nb;
		int line = 0;
		int section = 0;
		int path_arg = 0;

		if (v || l || p || f){
			printf("ERROR\nplease execute only one command at once\n");
			return;
		}

		// fetching command options, if any
		for(arg_nb = 2; arg_nb < argc; arg_nb++){
			if(strncmp(argv[arg_nb], "section=", 8) == 0){
				section = atoi(argv[arg_nb] + 8);
			}
			if(strncmp(argv[arg_nb], "line=", 5) == 0){
				line = atoi(argv[arg_nb] + 5);
			}
			if(strncmp(argv[arg_nb], "path=", 5) == 0){
				path_arg = arg_nb;
			}
		}

		// check wether command options found
		if (section == 0){
			printf("ERROR\ninvalid section\n");
			return;
		}
		if (line == 0){
			printf("ERROR\ninvalid line\n");
			return;
		}

		// call extracting procedure
		if (strncmp(argv[path_arg], "path=", 5) == 0){
			if(valid(argv[path_arg] + 5)){
				header head = read_SF(argv[path_arg] + 5);

				// if nb of sections does not comply
				if (head.nb_sections < section){
					printf("ERROR\ninvalid section\n");
					return;
				}

				// if SF valid
				if (valid_SF(head, 0) == 0){
					extract(head, argv[path_arg] + 5, section, line);
				}

				// if SF not valid
				else {
					printf("ERROR\ninvalid file\n");
					return;
				}
			}

			// if not a valid path
			else{
				printf("ERROR\ninvalid file\n");
				return;
			}
		}
		else{
			printf("ERROR\nplease provide a path\n");			// lack of path parameter
			return;
		}
	}

	// FIND ALL COMMAND
	else if(strcmp(argv[f], "findall") == 0){

		if (v || l || p || e){
			printf("ERROR\nplease execute only one command at once\n");
			return;
		}

		if(strncmp(argv[2], "path=", 5) == 0){

			if(valid(argv[2] + 5)){												// valid path
				findall(argv[2] + 5, 1);
			}
			else{
				printf("ERROR\ninvalid directory path\n");
				return;
			}
		}
		else{
			printf("ERROR\nplease provide a path\n");				// lack of path parameter
			return;
		}
	}

	// ANYTHING ELSE IS NOT A VALID COMMAND
	else{
		printf("ERROR\nno valid command name has been found\n");
		return;
	}
}

// MAIN PROCEDURE
int main(int argc, char **argv){
	if(argc >= 2){
		select_cmd(argc, argv);
	}
return 0;
}
