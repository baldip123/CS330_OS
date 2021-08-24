/*Name:Baldip Singh Bijlani
Roll no.170203*/

#include <dirent.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <stdio.h>
#include <string.h>
#include <limits.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdlib.h>

long int search_in_directory(char *fn){// A function to get the sizes of directories recursively
    DIR *dir;
    struct dirent *entry;
    char path[PATH_MAX];
    struct stat info;
    long int directory_size = 0;
    if ((dir = opendir(fn)) == NULL){//opendir fails
        perror("opendir() error");
    }
    else{
        while ((entry = readdir(dir)) != NULL){

            if (entry->d_name[0] != '.'){//the entries  . and .. in the dirent need to be avoided

                strcpy(path, fn);
                if(fn[strlen(fn)-1]!='/'){
                    strcat(path, "/");
                }
                strcat(path, entry->d_name);//get the final path to the subdirectory to open it
                if (stat(path, &info) != 0){//stat fails on the subdirectory,it means that either we dont have permissions or the file type is incompatibale with stat.
                }
                else if (S_ISDIR(info.st_mode)){//subdirectory is a directory.
                    directory_size += search_in_directory(path);//recursively finds the size of the subdirectory. 
                }
                else if (S_ISREG(info.st_mode)){//subdirectory is a file.
                    directory_size += info.st_size;//just add it's size to the totall directory size.
                }

            }
        }
        closedir(dir);
    }
    return directory_size;
}

int main(int argc, char **argv){

    DIR *dir;
    struct dirent *entry;
    struct stat info;
    int ch = '/' ;
    char final_add[strlen(argv[1])];
    char path[PATH_MAX];
    long int directory_size = 0;

    if(argc == 1){
        fprintf(stderr,"no argument given");
        exit(-1);
    }//if no filename is given

    strcpy(final_add,argv[1]);

    if(strcmp(argv[1],"/")){
        if(argv[1][strlen(argv[1])-1] == '/'){
            final_add[strlen(argv[1])-1] = '\0';
        }
    }//do string operations on the address given. 
    

    if(stat(final_add, &info)!=0){//find the type of file
        fprintf(stderr, "stat() error on %s: %s\n", final_add,strerror(errno));
        exit(-1);
    }
    else if(S_ISDIR(info.st_mode)){//the file is  a directory
        if ((dir = opendir(final_add)) == NULL){
            fprintf(stderr,"opendir() error on %s",final_add);
            exit(-1);
        }
        int num_sub = 0;

        while ((entry = readdir(dir)) != NULL){//find the number of immediate subdirectories of the root directory
            if ((entry->d_name[0] != '.')){
                num_sub++;
            }
        }

        long int size_arr[num_sub];
        char *name_arr[num_sub];
        int i = 0;
        long int inter;
        closedir(dir);

        if ((dir = opendir(final_add)) == NULL)//Since the dirent has reached the end,open the directory again
            perror("opendir() error ");


        while ((entry = readdir(dir)) != NULL){
            struct stat info_sub;

            if (entry->d_name[0] != '.'){

                int fd[2];
                if(pipe(fd) < 0){
                    perror("pipe");
                }//set up a pipe for communication between 2 processes.

                strcpy(path, final_add);
                if(final_add[strlen(final_add)-1]!='/'){

                    strcat(path, "/");
                }
                strcat(path, entry->d_name);//get the full to open the subdirectory.

                if (stat(path, &info_sub) != 0){

                    fprintf(stderr, "stat() error on %s: %s\n", path,strerror(errno));
                }
                else if (S_ISDIR(info_sub.st_mode)){//the subdirectory is a directory.

                    int pid =fork();

                    if(pid == 0){
                        close(fd[0]);
                        inter = search_in_directory(path);
                        write(fd[1],&inter,sizeof(inter));//writes the result to pipe
                        exit(0);
                    }
                    else if(pid == -1){
                        fprintf(stderr,"fork failed");
                        exit(-1);
                    }

                    else{  
                        close(fd[1]);
                        wait(NULL);
                        read(fd[0],&inter,sizeof(inter));//Gets the input from the pipe
                        size_arr[i] = inter;
                        name_arr[i] = entry->d_name;//Stores the sizes and names of subdirectories to print in hte future
                        directory_size += inter;
                    }
                    i++;
                }


                else if(S_ISREG(info_sub.st_mode)){//If the subdirectory is a file, just add it's size.
                    directory_size += info_sub.st_size;
                }

            }
        }
        if(strlen(final_add)!=1)
            printf("%s %ld\n",(strrchr(final_add,ch)+1),directory_size);
        else
            printf("%s %ld\n",final_add,directory_size);//Prints only the name of the final directory in the original address given
        for(int j =0;j<i;j++){
            printf("%s %ld\n",name_arr[j],size_arr[j]);//Prints the names and sizes of Immediate subdirectories.
        }

    }
    else if(S_ISREG(info.st_mode)){
        printf("%s is not a directory but the filesize is %ld",final_add,info.st_size);
    }
        
        
    return 0;
}
