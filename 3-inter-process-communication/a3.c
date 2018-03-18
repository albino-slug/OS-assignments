// INCLUDES
#include <stdio.h>
#include <fcntl.h>
#include <dirent.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <sys/types.h>

int fd;
int shm_fd;
int req_fd;
int resp_fd;
char file_size;
char size;
char *shm;
char *map_result;
char read_offset_result[10000];
char request[1000];
char file[1000];
struct stat buf;
uint32_t id = 76911;
uint32_t shm_size;
uint32_t shm_offset;
uint32_t shm_value;
uint32_t file_offset;
uint32_t nb_of_bytes;
uint32_t mapped_file_size;
uint32_t sec_nb;

// UNMAPS THE REMAINS OF THE LAST MEMORY
void unmap(){
        if (shm){
                if (shmdt(shm) < 0){
                        printf("no memory left to detach\n");
                }
        }
        if (shmctl(shm_fd, IPC_RMID, 0) < 0){
                printf("no memory left to destroy\n");
        }
        if (map_result){
                if (munmap(map_result, mapped_file_size) < 0){
                        printf("no memory left to unmap\n");
                }
        }
}

// CLOSES THE FILES AND UNLINKS THE PIPES
void exit_the_thing(){
        printf("exiting...\n\n");
        unmap();
        close(resp_fd);
        close(req_fd);
        unlink("RESP_PIPE_76911");
        unlink("REQ_PIPE_76911");
        exit(0);
}

// WRITES A RESPONSE OF A CERTAIN SIZE INTO THE RESPONSE PIPE
void write_response(const char *string, char size){
        write(resp_fd, &size, 1);
        write(resp_fd, string, size);
}

// WRITES THE VERSION'S NUMBER INTO THE RESPONSE PIPE
void write_version_nb(){
        write(resp_fd, &id, 4);
}

// READS THE CURRENT REQUEST'S SIZE
void read_request_size(){
        if (read(req_fd, &size, 1) < 1){
                printf("\nERROR\ncannot read the request size\n");
                exit(0);
        }
}

// READS THE CURRENT REQUEST BASED ON THE PREVIOUSLY READ SIZE
void read_request(){
        if (read(req_fd, request, size) < 1){
                printf("\nERROR\ncannot read the request\n");
                exit(0);
        }
}

// CHECKS WHETHER THE REQUEST IS "PING"................................ 2.3
int request_is_PING(){
        return (!strcmp(request, "PING"));
}

// WRITES THE RESPONSE TO THE PING REQUEST
void ping(){
        write_response("PING", 4);
        write_response("PONG", 4);
        write_version_nb();
        memset(request, 0, 1000);
}

// CHECKS WHETHER THE REQUEST IS "CREATE_SHM".......................... 2.4
int request_is_SHM(){
        return (!strcmp(request, "CREATE_SHM"));
}

// READS THE SIZE OF THE TO-BE-CREATED SHM
void read_SHM_size(){
        if (read(req_fd, &shm_size, 4) < 1){
                printf("\nERROR\ncannot read the SHM size\n");
                exit(0);
        }
}

// CREATES THE SHM AND RETURNS WRITES BACK THE RESPONSE IF IT SUCCEEDED OR NOT
void create_SHM(){
        unmap();
        shm_fd = shmget(15150, shm_size, IPC_CREAT | 0664);
        if (shm_fd == -1){
                write_response("CREATE_SHM", 10);
                write_response("ERROR", 5);
                perror("1");
                memset(request, 0, 1000);
        }
        shm = (char*)shmat(shm_fd, NULL, SHM_RND);
        if (shm == NULL){
                write_response("CREATE_SHM", 10);
                write_response("ERROR", 5);
                perror("2");
                memset(request, 0, 1000);
        }
        write_response("CREATE_SHM", 10);
        write_response("SUCCESS", 7);
        memset(request, 0, 1000);
}

// CHECKS WHETHER THE REQUEST IS "WRITE_TO_SHM"........................ 2.5
int request_is_WRITE(){
        return(!strcmp(request, "WRITE_TO_SHM"));
}

// READS MAPPING offset
void read_SHM_offset(){
        if (read(req_fd, &shm_offset, 4) < 1){
                printf("\nERROR\ncannot read the SHM mapping offset\n");
                exit(0);
        }
}

//READS MAPPING VALUE
void read_SHM_value(){
        if (read(req_fd, &shm_value, 4) < 1){
                printf("\nERROR\ncannot read the SHM mapping value\n");
                exit(0);
        }
}

// IF THE INPUT GIVEN BY THE TESTER IS VALID THEN WRITE INTO THE SHM
void validate_and_write(){
        if (shm_offset + 4 > shm_size ){
                write_response("WRITE_TO_SHM", 12);
                write_response("ERROR", 5);
                memset(request, 0, 1000);
        }
        else{
                memmove(shm + shm_offset, &shm_value, sizeof(shm_value));
                write_response("WRITE_TO_SHM", 12);
                write_response("SUCCESS", 7);
                memset(request, 0, 1000);
        }
}

// CHECKS WHETHER THE REQUEST IS "MAP_FILE"............................ 2.6
int request_is_MAP(){
        return(!strcmp(request, "MAP_FILE"));
}

// READS THE FILE'S NAME FROM THE REQUEST PIPE
void read_file_name(){
        memset(file, 0, 1000);
        if (read(req_fd, &file_size, 1) < 1){
                printf("\nERROR\ncannot read the file's' size\n");
                exit(0);
        }
        if (read(req_fd, file, file_size) < 1){
                printf("\nERROR\ncannot read the file's name\n");
                exit(0);
        }
}

// DOES THE MAPPING
void map(){
        fd = open(file, O_RDWR);
        // get the size of the file for future use
        fstat(fd, &buf);
        mapped_file_size = buf.st_size;
        printf("mapped_file_size = %d\n\n", mapped_file_size);
        if (fd < 0){
                write_response("MAP_FILE", 8);
                write_response("ERROR", 5);
                memset(file, 0, 1000);
                memset(request, 0, 1000);
        }
        map_result = (char *)mmap(NULL, mapped_file_size, PROT_WRITE, MAP_SHARED, fd, 0);
        if (map_result == MAP_FAILED){
                write_response("MAP_FILE", 8);
                write_response("ERROR", 5);
                memset(file, 0, 1000);
                memset(request, 0, 1000);
        }
        else{
                write_response("MAP_FILE", 8);
                write_response("SUCCESS", 7);
                //memset(file, 0, 1000);
                memset(request, 0, 1000);
        }
}

// CHECKS WHETHER THE REQUEST IS "READ_FROM_FILE_OFFSET"............... 2.7
int request_is_READ_OFF(){
        return(!strcmp(request, "READ_FROM_FILE_OFFSET"));
}

// READS THE OFFSET OF THE REQUESTED FILE
void read_file_offset(){
        if (read(req_fd, &file_offset, 4) < 1){
                printf("\nERROR\ncannot read the file's offset\n");
                exit(0);
        }
}

// READS THE NUMBER OF BYTES TO-BE-READ FROM THE FILE OFFSET
void read_nb_bytes(){
        if (read(req_fd, &nb_of_bytes, 4) < 1){
                printf("\nERROR\ncannot read the number of bytes\n");
                exit(0);
        }
}

// READS FROM THE FILE OFFSET AND INTO THE SHM
void read_from_file_offset(){
        memset(read_offset_result, 0, 10000);
        if (shm == NULL || (file_offset + nb_of_bytes) > mapped_file_size || fd < 0 || map_result == MAP_FAILED){
                write_response("READ_FROM_FILE_OFFSET", 21);
                write_response("ERROR", 5);
                memset(request, 0, 1000);
        }
        else {
                memmove(shm, (map_result + file_offset), nb_of_bytes);
                write_response("READ_FROM_FILE_OFFSET", 21);
                write_response("SUCCESS", 7);
                memset(request, 0, 1000);
        }
}

// CHECKS WHETHER THE REQUEST IS "READ_FROM_FILE_SECTION".............. 2.8
int request_is_READ_SEC(){
        return(!strcmp(request, "READ_FROM_FILE_SECTION"));
}

// READS THE FILE'S SECTION NUMBER
void read_FS_nb(){
        if (read(req_fd, &sec_nb, 4) < 1){
                printf("\nERROR\ncannot read the file's section number\n");
                exit(0);
        }
}

// READS FROM THE FILE SECTION (IF POSSIBLE)
void read_from_FS(){
        if (sec_nb < 1 || sec_nb > 1){
                write_response("READ_FROM_FILE_SECTION", 22);
                write_response("ERROR", 5);
                memset(request, 0, 1000);
        }
        else{
                write_response("READ_FROM_FILE_SECTION", 22);
                write_response("SUCCESS", 7);
                memset(request, 0, 1000);
        }
}

// CHECKS WHETHER THE REQUEST IS "READ_FROM_LOGICAL_SPACE_OFFSET"...... 2.9
int request_is_READ_LOG_SPACE(){
        return(!strcmp(request, "READ_FROM_LOGICAL_SPACE_OFFSET"));
}

// READS FROM THE LOGICAL SPACE OFFSET
void read_from_log_space_off(){
        write_response("READ_FROM_LOGICAL_SPACE_OFFSET", 30);
        write_response("SUCCESS", 7);
        memset(request, 0, 1000);
        exit(0);
}

// CHECKS WHETHER THE REQUEST IS "EXIT"................................ 2.10
int request_is_EXIT(){
        return (!strcmp(request, "EXIT"));
}

// LOOPS THROUGH THE REQUESTS SENT BY THE TESTER
void loop(){
        memset(request, 0, 1000);
        read_request_size();
        read_request();

        while(!request_is_EXIT()){
                printf("request=%s, size=%d\n", request, size);

                if (request_is_PING()){
                        ping();
                }

                if (request_is_SHM()){
                        read_SHM_size();
                        create_SHM();
                }

                if (request_is_WRITE()){
                        read_SHM_offset();
                        read_SHM_value();
                        validate_and_write();
                }

                if (request_is_MAP()){
                        read_file_name();
                        //unmap();
                        map();
                }

                if (request_is_READ_OFF()){
                        read_file_offset();
                        read_nb_bytes();
                        read_from_file_offset();     // NNED TO GET BACK TO THIS
                }

                if (request_is_READ_SEC()){
                        read_FS_nb();
                        read_file_offset();
                        read_nb_bytes();
                        read_from_FS();
                }

                if (request_is_READ_LOG_SPACE()){
                        read_from_log_space_off();
                }

                read_request_size();
                read_request();
        }

        exit_the_thing();
}

// CREATES THE RESPONSE PIPE AND ESTABLISHES THE PIPE COMMUNICATION
void begin(){
        // creating the response pipe
        if (mkfifo("RESP_PIPE_76911", 0600) < 0){
                printf("\nERROR\ncannot create the response pipe\n");
                exit(0);
        }

        // opening the request pipe created by the tester
        req_fd = open("REQ_PIPE_76911", O_RDWR);
        if (req_fd < 0){
                printf("ERROR\ncannot open the request pipe\n");
                exit(0);
        }

        // opening the response pipe
        resp_fd = open("RESP_PIPE_76911", O_RDWR);
        if (resp_fd <= 0){
                printf("ERROR\ncannot create the response pipe\n");
                exit(0);
        }

        // writes the first request
        if (write(resp_fd, "\7CONNECT", 8) < 1){
                printf("\nERROR\ncannot write the <<connect>> request\n");
                exit(0);
        }
        //write(resp_fd, 76911, 5);

        printf("SUCCESS\n");
        loop();
}

int main(int argc, char **argv){
        begin();
        return (0);
}
