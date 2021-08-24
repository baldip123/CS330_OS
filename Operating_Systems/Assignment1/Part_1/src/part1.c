/*Name:Baldip Singh Bijlani
170203*/
#include <dirent.h>
#include <errno.h>
#include <sys/types.h>
#include <stdio.h>
#include <string.h>
#include <limits.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdlib.h>
void search_in_file(char *path_to_print,char *file_name, char * string,int flag)
{
    int return_value, fd;
    char buffer[1];
    char line[1000000];//buffer
    if((fd = open(file_name, O_RDONLY)) < 0)//error in opening the file
        perror(strcat("open() error ",file_name));
    else{
        int i =0;
        while(read(fd, buffer, 1)>0){//keep reading till end of file 
            if(buffer[0]!='\n')//keep reading till finding a newline charecter
            {
                line[i] = buffer[0];
                i++;
            }
            else//compare as soon as you see a new line charecter
            {
                line[i] = buffer[0];
                i++;
                line[i] = '\0';
                char *p = strstr(line,string);
                if(p&&flag)//if comparision is positive print the line along with the path to the file
                    printf("%s:%s",path_to_print,line);
                else if(p)//takes care of the case where only the line and not the path needs to be printed.
                    printf("%s",line);
                i = 0;
            }
            
        }

    }
}

void search_in_directory(char * path_to_print,char *fn, char *string) {
  DIR *dir;
  struct dirent *entry;
  char path[PATH_MAX];
  struct stat info;

  if ((dir = opendir(fn)) == NULL)
    perror("opendir() error");
  else {
    while ((entry = readdir(dir)) != NULL) {
      if (entry->d_name[0] != '.') {
        char *new_path = (char*)malloc((strlen(path_to_print)+strlen(entry->d_name))*sizeof(char)*2);
        strcpy(path, fn);
        strcpy(new_path, path_to_print);
        if(fn[strlen(fn)-1]!='/'){
            strcat(path, "/");
        }
        if(new_path[strlen(new_path)-1]!='/'){
            strcat(new_path, "/");
        }
        strcat(path, entry->d_name);//get the path in relative reference to the currentdirectory in order to avoid large path lengths
        strcat(new_path, entry->d_name);//get the path to be printed
        if (stat(path, &info) != 0)
            fprintf(stderr, "stat() error on %s: %s\n", path,
                  strerror(errno));
        else if (S_ISDIR(info.st_mode)){//directory
            if (chdir(path) != 0)//change the directory to the subdirectory
                fprintf(stderr,"chdir() to %s failed",path);
            search_in_directory(new_path,"./", string);//search in the subdirectory
            if (chdir("./..") != 0)
                fprintf(stderr,"chdir() to ./.. failed");//go back to the directory at the start of the function call
        }
        else if (S_ISREG(info.st_mode)){// file: just search in it.
            search_in_file(new_path,path, string, 1);

        }
      }
    }
    closedir(dir);
  }
}


int main(int argc, char **argv)
{
    DIR *dir;
    struct dirent *entry;
    struct stat info;
    //check if it is a file and handle accordingly.
    if(stat(argv[2], &info)!=0)
        fprintf(stderr, "stat() error on %s: %s\n", argv[2],strerror(errno));
    else if(S_ISDIR(info.st_mode)){
        if (chdir(argv[2]) != 0)//go to the directory
                fprintf(stderr,"chdir() to %s failed",argv[2]);
        search_in_directory(argv[2],"./", argv[1]);//search in the directory
    }
        
    else if(S_ISREG(info.st_mode))
        search_in_file(argv[2],argv[2], argv[1], 0);//search the file

    return 0;
}
