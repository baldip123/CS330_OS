#include<pipe.h>
#include<context.h>
#include<memory.h>
#include<lib.h>
#include<entry.h>
#include<file.h>
/***********************************************************************
 * Use this function to allocate pipe info && Don't Modify below function
 ***********************************************************************/
struct pipe_info* alloc_pipe_info()
{
    struct pipe_info *pipe = (struct pipe_info*)os_page_alloc(OS_DS_REG);
    char* buffer = (char*) os_page_alloc(OS_DS_REG);
    pipe ->pipe_buff = buffer;
    return pipe;
}


void free_pipe_info(struct pipe_info *p_info)
{
    if(p_info)
    {
        os_page_free(OS_DS_REG ,p_info->pipe_buff);
        os_page_free(OS_DS_REG ,p_info);
    }
}
/*************************************************************************/
/*************************************************************************/



long pipe_close(struct file *filep)
{
    /**
    * TODO:: Implementation of Close for pipe 
    * Free the pipe_info and file object
    * Incase of Error return valid Error code 
    */
    int ret_fd = -EINVAL; 
    //Check if it is indeed a pipe
    
    
    return ret_fd;
}



int pipe_read(struct file *filep, char *buff, u32 count)
{
    /**
    *  TODO:: Implementation of Pipe Read
    *  Read the contect from buff (pipe_info -> pipe_buff) and write to the buff(argument 2);
    *  Validate size of buff, the mode of pipe (pipe_info->mode),etc
    *  Incase of Error return valid Error code 
    */
    int ret_fd = -EINVAL; 
    //Check if it is indeed a pipe

    if(filep == NULL || buff == NULL)
        return -EINVAL;
    if(filep->pipe==NULL)
        return -EINVAL;

    //Check if the pipe has any descriptor with read to it
    if(!filep->pipe->is_ropen)//You must update this value when it closes
        return -EACCES;



    int read_idx = filep->pipe->read_pos;
    int write_idx = filep->pipe->write_pos;
    int size = filep->pipe->buffer_offset;

    if(size<0||size>PIPE_MAX_SIZE)
        return -EOTHERS;

    if(count>((u32)(size)))
        count = size;
        
    int i =0;
    while(i<count)
    {
        buff[i] = filep->pipe->pipe_buff[(read_idx+i)%PIPE_MAX_SIZE];
        size--;//the size keeps on decreasing
        i++;
    }

    filep->pipe->read_pos = ((filep->pipe->read_pos)+count)%PIPE_MAX_SIZE;
    filep->pipe->buffer_offset-=count;

    return  count;
}


int pipe_write(struct file *filep, char *buff, u32 count)
{
    /**
    *  TODO:: Implementation of Pipe Read
    *  Write the contect from   the buff(argument 2);  and write to buff(pipe_info -> pipe_buff)
    *  Validate size of buff, the mode of pipe (pipe_info->mode),etc
    *  Incase of Error return valid Error code 
    */
    int ret_fd = -EINVAL;
    if(filep ==  NULL)
        return -EINVAL;

    if(filep->pipe==NULL)
        return -EINVAL;

    //Check if the pipe has any descriptor with read to it
    if(!filep->pipe->is_wopen)//You must update this value when it closes
        return -EACCES;


    int write_idx = filep->pipe->write_pos;
    int size = filep->pipe->buffer_offset;

    if(size<0||size>PIPE_MAX_SIZE)
        return 0;

    if(count+((u32)size)>((u32)(PIPE_MAX_SIZE))) return -EINVAL;
    else
    {
        int i =0;
        while(i<count)
        {
            filep->pipe->pipe_buff[(write_idx+i)%PIPE_MAX_SIZE] = buff[i];
            size++;//the size keeps on increasing
            i++;
        }

        filep->pipe->write_pos = (write_idx+count)%PIPE_MAX_SIZE;
        filep->pipe->buffer_offset += count;
        return  i;
    }
    
    return ret_fd;
}

int create_pipe(struct exec_context *current, int *fd)
{
    /**
    *  TODO:: Implementation of Pipe Create
    *  Create file struct by invoking the alloc_file() function, 
    *  Create pipe_info struct by invoking the alloc_pipe_info() function
    *  fill the valid file descriptor in *fd param
    *  Incase of Error return valid Error code 
    */
    int ret_fd = -EINVAL;

    //THe argument may be invalid
    if(current==NULL)
        return -EINVAL;


    //Getting two valid file descriptors and returning error otherwise.
    fd[0] = 3;
    while((fd[0]<MAX_OPEN_FILES)&&(current->files[fd[0]] != NULL))
        fd[0]++;
    if(fd[0] >= MAX_OPEN_FILES)
        return -ENOMEM;
    
    fd[1] = fd[0]+1;
    while((fd[1]<MAX_OPEN_FILES)&&(current->files[fd[1]] != NULL))
        fd[1]++;
    if(fd[1] >= MAX_OPEN_FILES)
        return -ENOMEM;
    //Got two valid file descriptors.


    //1.//Allocating a file object for read
    struct file* filep_read = alloc_file();
    //Checking if was alllocated properly
    if(filep_read==NULL)
        return -ENOMEM;
    //It worked properly
    filep_read->type = PIPE;
    filep_read->mode = O_READ ;//SEE WHAT IT NEEDS TO BE;
    filep_read->offp = 0;//I dont think it matters
    filep_read->ref_count = 1;
    filep_read->inode = NULL;
    if( (filep_read->pipe = alloc_pipe_info())==NULL )//pipe_info failed
        return -ENOMEM;
    filep_read->fops->read = pipe_read;
    filep_read->fops->close = generic_close;


    //2.//Allocating a file object for write
    struct file* filep_write = alloc_file();
    //Checking if was alllocated properly
    if(filep_write==NULL)
        return -ENOMEM;
    //It worked properly
    filep_write->type = PIPE;
    filep_write->mode = O_WRITE ;//SEE WHAT IT NEEDS TO BE;
    filep_write->offp = 0;//I dont think it matters
    filep_write->ref_count = 1;
    filep_write->inode = NULL;
    filep_write->pipe = filep_read->pipe;
    filep_write->fops->write = pipe_write;
    filep_write->fops->close = generic_close;



    //filling the required info in pipe_info
    filep_read->pipe->read_pos = 0;
    filep_read->pipe->write_pos = 0;
    filep_read->pipe->buffer_offset = 0;
    filep_read->pipe->is_ropen = 1;//to show that read is open.
    filep_read->pipe->is_wopen = 1;//To show that write is open.



    //assign the file objects to the file descriptors
    current->files[fd[0]] = filep_read;
    current->files[fd[1]] = filep_write;
    return 0;   
}


