/*Name:Baldip Singh Bijlani
Roll no.170203*/
#define _POSIX_SOURCE
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <string.h>
extern char **environ;

void op_at_the_rate(char *string, char * file){
    int pid, fd[2];
    if(pipe(fd) < 0)//error checking in pipe
        perror("pipe");
    pid = fork();
    if(pid == 0){//child
        dup2(fd[1],1);
        close(fd[0]);
        close(fd[1]);//STDOUT(CHILD)->PIPE.
        char *cmd[]={"/bin/grep","-r", "-F", string,file,NULL};
        execvp("/bin/grep" , cmd);//executing grep sends the output to the pipe.
    }
    else if(pid == -1){//failure in fork
        perror("fork() error");
    }
    else{//parent 
        dup2(fd[0],0);
        close(fd[0]);
        close(fd[1]);//PIPE->STDOUT(PARENT).
        //wait(NULL);
        char *cmd[]={"/usr/bin/wc","-l",NULL};
        execvp("/usr/bin/wc" , cmd);//gets the input from pipe.Also since where wc is not known,
        //If the firstexec fails I call it again assuming wc is in bin.
        char *cmd2[] = {"/bin/wc","-l",NULL};
        execvp("/bin/wc" , cmd2);
    }
}
void grep_tee_command(int argc, char **argv){
    int pid, fd[2];
    if(pipe(fd) < 0)
        perror("pipe() error");
    pid = fork();
    if(pid == 0){//child
        dup2(fd[1],1);
        close(fd[0]);
        close(fd[1]);//STDOUT(CHILD)->PIPE.
        char *cmd[]={"/bin/grep","-r", "-F", argv[2],argv[3],NULL};
        execvp("/bin/grep" , cmd);
    }
    else if(pid == -1){//failure in fork
        perror("fork() error");
    }
    else{
        dup2(fd[0],0);
        close(fd[0]);
        close(fd[1]);
        int fd2[2];//create a second pipe.
        if(pipe(fd2) < 0)
            perror("pipe() error");
        pid = fork();//do a fork again
        if(pid == 0){//second child
            int BUFF_SIZE = 5;
            char *buff[BUFF_SIZE];
            int bytes_read = BUFF_SIZE;
            dup2(fd2[1],1);
            close(fd2[0]);
            close(fd2[1]);//OUTPUT(CHILD_1)->PIPE_1->PIPE_2->FILE and Inout of the comman.
            //
            int tee_file = open(argv[4],O_RDWR | O_CREAT | O_TRUNC, 0666 );
            if(tee_file < 0){
                fprintf(stderr,"open() error on %s",argv[4]);
            }//open the file and check for errors

            while((bytes_read = read(0, buff, BUFF_SIZE)) > 0){//reading from the pipe the output of the first child
                if(write(tee_file, buff, bytes_read) != bytes_read){//Writing to the file
					perror("Couldn't write all bytes to file");
					exit(-1);
				}
                if(write(1, buff, bytes_read) != bytes_read) {//writing to the PIPE_2
					perror("Couldn't write all bytes to stdout");
					exit(-1);
				}
            }
            close(tee_file);
        }
        else if(pid == -1){
            perror("fork() error");
        }
        else{
            dup2(fd2[0],0);
            close(fd2[0]);
            close(fd2[1]);//Output of the pipe becomes the input of this process.
            char *cmd[argc-3];
            char instruction[11+strlen(argv[5])];
            cmd[0] = strcat(strcpy(instruction,"/usr/bin/"),argv[5]);
            for(int i = 6;i <= argc;i++){
                cmd[i-5] = argv[i]; 
            }
            execvp(cmd[0] , cmd);
            //if control reaches here means exec failed, hence I change the path of the command and try again.
            char *cmd2[argc-3];
            for(int i = 1;i<argc-3;i++){
                cmd2[i] = cmd[i];
            }
            cmd2[0] = strcat(strcpy(instruction,"/bin/"),argv[5]);
            execvp(cmd2[0] , cmd2);
        }
    }
}
int main(int argc, char **argv)
{
    if(!strcmp(argv[1],"@")){//check which os the following two parts(a or b )have to be performed?
        op_at_the_rate(argv[2], argv[3]);//arg2 is the string and arg3 the directory
    }

    else if(!strcmp(argv[1],"$")){
        grep_tee_command(argc,argv);
    }

    return 0;
}
