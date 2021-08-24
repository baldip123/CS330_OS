#include<types.h>
#include<context.h>
#include<file.h>
#include<lib.h>
#include<serial.h>
#include<entry.h>
#include<memory.h>
#include<fs.h>
#include<kbd.h>
#include<pipe.h>


/************************************************************************************/
/***************************Do Not Modify below Functions****************************/
/************************************************************************************/
void free_file_object(struct file *filep)
{
    if(filep)
    {
       os_page_free(OS_DS_REG ,filep);
       stats->file_objects--;
    }
}

struct file *alloc_file()
{
  
  struct file *file = (struct file *) os_page_alloc(OS_DS_REG); 
  file->fops = (struct fileops *) (file + sizeof(struct file)); 
  bzero((char *)file->fops, sizeof(struct fileops));
  stats->file_objects++;
  return file; 
}

static int do_read_kbd(struct file* filep, char * buff, u32 count)
{
  kbd_read(buff);
  return 1;
}

static int do_write_console(struct file* filep, char * buff, u32 count)
{
  struct exec_context *current = get_current_ctx();
  return do_write(current, (u64)buff, (u64)count);
}

struct file *create_standard_IO(int type)
{
  struct file *filep = alloc_file();
  filep->type = type;
  if(type == STDIN)
     filep->mode = O_READ;
  else
      filep->mode = O_WRITE;
  if(type == STDIN){
        filep->fops->read = do_read_kbd;
  }else{
        filep->fops->write = do_write_console;
  }
  filep->fops->close = generic_close;
  filep->ref_count = 1;
  return filep;
}

int open_standard_IO(struct exec_context *ctx, int type)
{
   int fd = type;
   struct file *filep = ctx->files[type];
   if(!filep){
        filep = create_standard_IO(type);
   }else{
         filep->ref_count++;
         fd = 3;
         while(ctx->files[fd])
             fd++; 
   }
   ctx->files[fd] = filep;
   return fd;
}
/**********************************************************************************/
/**********************************************************************************/
/**********************************************************************************/
/**********************************************************************************/



void do_file_fork(struct exec_context *child)
{
   /*TODO the child fds are a copy of the parent. Adjust the refcount*/
    if(child == NULL)
      return ;
    for(int i =0;i<MAX_OPEN_FILES;i++)
    {
      if (child->files[i]!=NULL)
      {
        child->files[i]->ref_count = child->files[i]->ref_count+1;//close all the files if they arent closed already.
      }
     
    }
 
}

void do_file_exit(struct exec_context *ctx)
{
   /*TODO the process is exiting. Adjust the ref_count
     of files*/

    //check the ctx
    if(ctx == NULL)
      return ;
    for(int i =0;i<MAX_OPEN_FILES;i++)
    {
      if (ctx->files[i]!=NULL)
      {
        ctx->files[i]->fops->close(ctx->files[i]);//close all the files if they arent closed already.
        ctx->files[i]=NULL;
      }
     
    }
}

long generic_close(struct file *filep)
{
  /** TODO Implementation of close (pipe, file) based on the type 
   * Adjust the ref_count, free file object
   * Incase of Error return valid Error code 
   */

  //current inmplementation for file on;y
    int ret_fd = -EINVAL; 

    if(filep == NULL)//filep is invalid
      return -EINVAL;

    if( filep->type != PIPE )//Not Pipe
    {
      if(filep->ref_count == 1)//Only 1 file_descriptor points to it which will be removed
        free_file_object(filep);
      else if(filep->ref_count >1)
        (filep->ref_count)--;//We assume that fd in the fd table starts pointing to null  itself
      return  0;
    }
    else//file object is for pipe
    {
        if(filep->ref_count == 1)//Only 1 file_descriptor points to it which will be removed
        {
          if( filep->mode == O_READ )
          {
            filep->pipe->is_ropen = 0;//as no file descriptors remain to read from it.
            if(filep->pipe->is_wopen == 0)//both the ends are closed.
            {
              //remove the pipe
              free_pipe_info(filep->pipe);
            }
          }
          else if (filep->mode == O_WRITE)
          {
            filep->pipe->is_wopen = 0;//as no file descriptors remain to read from it.
            if(filep->pipe->is_ropen == 0)//both the ends are closed.
            {
              //remove the pipe
              free_pipe_info(filep->pipe);
            }
          }
          
          free_file_object(filep);
        }
        else if(filep->ref_count >1)
          (filep->ref_count)--;
        return  0;
    }
    
    
    return ret_fd;
}

static int do_read_regular(struct file *filep, char * buff, u32 count)
{
   /** TODO Implementation of File Read, 
    *  You should be reading the content from File using file system read function call and fill the buf
    *  Validate the permission, file existence, Max length etc
    *  Incase of Error return valid Error code 
    * */
    int ret_fd = -EINVAL;

    if(filep == NULL)
      return -EINVAL;

    if(filep->inode == NULL)
      return -EINVAL;

    if( ((filep->mode)&O_READ) == 0 )//NO PREMISSION TO READ//CAN BE CAUSE FOR ERROR
      return -EACCES;//Checking from only the file as inode may have other value

    ret_fd = filep->inode->read(filep->inode, buff, count, &(filep->offp) );
    //modify the value of the offset
    filep->offp += ret_fd;
    return ret_fd;
}


static int do_write_regular(struct file *filep, char * buff, u32 count)
{
    /** TODO Implementation of File write, 
    *   You should be writing the content from buff to File by using File system write function
    *   Validate the permission, file existence, Max length etc
    *   Incase of Error return valid Error code 
    * */
    int ret_fd = -EINVAL; 

    if(filep == NULL)
      return -EINVAL;
    
    if(filep->inode == NULL)
      return -EINVAL;

    if( ((filep->mode)&O_WRITE) == 0 )//NO PREMISSION TO READ//CAN BE CAUSE FOR ERROR
      return -EACCES;//Checking from only the file as inode may have other value

    ret_fd = filep->inode->write(filep->inode, buff, count, &(filep->offp) );
    //modify the value of the offset if it writes properly
    if(ret_fd == -1)//else return only
      return -1;
    else
      filep->offp += ret_fd;

    return ret_fd;
}

static long do_lseek_regular(struct file *filep, long offset, int whence)
{
  //Can have error in the setting and returning of offsets.
    /** TODO Implementation of lseek 
    *   Set, Adjust the ofset based on the whence
    *   Incase of Error return valid Error code 
    * */
    int ret_fd = -EINVAL; 
    //Cheking for valid filepointer
    if (filep==NULL)
      return -EINVAL;

    //Checking for valid whence
    if((whence!=SEEK_CUR)&&(whence!=SEEK_END)&&(whence!=SEEK_SET))
      return -EINVAL;
    
    //checking if filepointer belongs to file or pipe
    if((filep->pipe)!=NULL)
      return -EOTHERS;

    long int curr_end = (long int)(filep->inode->max_pos);//exclusive: meaning the end of file + 1.
    long int start = (long int)(filep->inode->s_pos);//int should work fine as it is well within limit
    long int end = (long int)(filep->inode->e_pos);
    long int max_size = (long int)(filep->inode->e_pos - filep->inode->s_pos);
    long int curr_size  = (long int)(curr_end - start);

    if(whence == SEEK_SET)
    {
      if( (offset >= max_size) || (offset<0) )//It causes it to go out of bounds
        return -EINVAL;
      // else if(offset<=curr_size)//All is normal
      // {
      //   filep->offp = offset;
      //   return offset;
      // }
      // else
      // {
      //   char buff[offset-curr_size];
      //   for(int i=0;i<offset-curr_size;i++)
      //   {
      //     buff[i] = '\0';
      //   }
      //   unsigned int bits_wrote = do_write_regular(filep,buff,offset-curr_size);

      //   if(bits_wrote!=offset-curr_size)
      //     return -EOTHERS;

      //   filep->inode->max_pos = curr_end;
      //   filep->inode->file_size = filep->inode->max_pos - filep->inode->s_pos;
      //   filep->offp = offset;
      //   return offset;
      // }
      else
      {
        filep->offp = offset;
        return offset;
      }
      /** else if(offset+filep->inode->s_pos>filep->inode->max_pos)
      {
        unsigned int curr_end = filep->inode->max_pos;
        unsigned int curr_start = filep->inode->s_pos;
        unsigned int max_size = filep->inode->e_pos - filep->inode->s_pos;
        char buff[filep->inode->max_pos - offset+filep->inode->s_pos];
        for(int i = 0;i<filep->inode->max_pos - offset+filep->inode->s_pos;i++)
        {
          buff[i] = '\0';
        }
        filep->inode->write(filep->inode,buff,filep->inode->max_pos - offset+filep->inode->s_pos,filep->inode->max_pos-filep->inode->s_pos);
        filep->inode->max_pos = //revert
        filep->inode->min = //revert
      }
      * */      
    }
    if(whence == SEEK_END)
    {
      if( (curr_end+offset < start) || (curr_end+offset>end) )//It causes it to go out of bounds
        return -EINVAL;
      else
      {
        filep->offp = (u32)(curr_size+offset);
        return curr_size+offset;
      }
         
    }
    if(whence == SEEK_CUR)
    {
      if( (((long int)filep->offp)+offset < 0) || (((long int)filep->offp)+offset>max_size) )//It causes it to go out of bounds
        return -EINVAL;
      else
      {
        filep->offp = (u32)(((long int)filep->offp)+offset);
        return (((long int)filep->offp)+offset);
      }
         
    }

    
    
    return ret_fd;
}

extern int do_regular_file_open(struct exec_context *ctx, char* filename, u64 flags, u64 mode)
{ 
  /**  TODO Implementation of file open, 
    *  You should be creating file(use the alloc_file function to creat file), 
    *  To create or Get inode use File system function calls, 
    *  Handle mode and flags 
    *  Validate file existence, Max File count is 32, Max Size is 4KB, etc
    *  Incase of Error return valid Error code 
    * */
    int ret_fd = -EINVAL; 
    struct inode* found_inode = lookup_inode(filename);
    struct inode* new_inode;
    /** IF INODE IS NOT FOUND , I CREATE A NEW INODE.
     *      IF O_CREAT IS 0, I RETRUN
     * ELSE I CHECK IF THE FLAGS AND MODE OF INODE ARE COMPAT
     * */

    if (found_inode==NULL)//The inode does not exist.
    {

      if( (O_CREAT & flags) != 0 )//The flags have O_CREAT bit as 1.
      {
        new_inode = create_inode(filename,mode);
        if(new_inode == NULL)//ERROR IN CREATING INODE
        {
          return -EINVAL;
        }
      }
      else//IF new inode is not to be created,return.
      {
        return -EINVAL;
      }
      
    }
    else//THE INODE WAS FOUND,Checking if the permission are compat
    {
      // u32 found_mode = found_inode->mode;
      if( ((O_READ & flags)) && (!(O_READ & (found_inode->mode) )) )//READ PERMISSION IS NOT THERE
        return -EACCES;
      else if ( ((O_WRITE & flags)) && (!(O_WRITE & (found_inode->mode) )) )//WRITE PERMISSION IS NOT THERE
        return -EACCES;
    }

    //AT THIS PART OF THE CODE, I ALREADY HAVE AN INODE,WHICH IS VALID.

    /** Finding a free file descriptor,
     * Retrunning if none exist
     * */
    int free_fd = 3;
    while(( (free_fd<MAX_OPEN_FILES) && (ctx->files[free_fd]!=NULL) ))
      free_fd++;
    if(free_fd == 32)//no free_fd was found
      return -ENOMEM;
    
    //I HAVE A FREE FILE DESCIPTOR HERE.

    /** Hence I must return a file descriptor as 
     * I have a valid free fd
     * I have a valid inode with all the permissions.
     * */

    struct file* filep = alloc_file();

    if(filep == NULL)
      return -ENOMEM;
    
    //FILLING THE FIELDS OF THE FILE OBJECT.
    filep->type = (found_inode == NULL) ? REGULAR : found_inode->type;//POTENTIAL CAUSE FOR ERROR !!!!!
    filep->mode =  (found_inode == NULL) ? mode : flags;//POTENTIAL CAUSE FOR ERROR !!!!!
    filep->offp = 0;
    filep->ref_count = 1;
    filep->inode =  (found_inode == NULL) ? new_inode : found_inode;//POTENTIAL CAUSE FOR ERROR !!!!!
    filep->pipe = NULL;
    //NOW IS SET THE FUNCTIONS BASED ON THE INODE PERMISSIONS.

    if(found_inode!=NULL)
    {
      if( ( (found_inode->mode) & O_READ)!=0 )//requires the read
        filep->fops->read = do_read_regular;
      if( ( (found_inode->mode) & O_WRITE)!=0 )//requires the read
        filep->fops->write = do_write_regular;
      filep->fops->close = generic_close;//Have to add this function
      filep->fops->lseek = do_lseek_regular;
      
    }
    else
    {
      if( ( (new_inode->mode) & O_READ)!=0 )//requires the read
        filep->fops->read = do_read_regular;
      if( ( (new_inode->mode) & O_WRITE)!=0 )//requires the read
        filep->fops->write = do_write_regular;
      filep->fops->close = generic_close;//Have to add this function
      filep->fops->lseek = do_lseek_regular;
    }

    //WE HAVE CORRECTLY SET THE FILE OBJECT AS WELL

    //NOW WE ASSIGN THE FILE OBJECT TO FD;

    ctx->files[free_fd] = filep;
    ret_fd = free_fd;
    return ret_fd;
}

int fd_dup(struct exec_context *current, int oldfd)
{
     /** TODO Implementation of dup 
      *  Read the man page of dup and implement accordingly 
      *  return the file descriptor,
      *  Incase of Error return valid Error code 
      * */
    int ret_fd = -EINVAL; 
    if(current == NULL)
      return -EINVAL;
    

    //Check if the old file descriptor is within range
    if((oldfd>=MAX_OPEN_FILES)||oldfd<0)
      return -EINVAL;

    //Check if oldfd is an open file descriptor
    if( (current->files[oldfd])==NULL )
      return -EOTHERS;//DISCUSS WITH SOMEONE

    //FIND AN EMPTY FILE DESCRIPTOR
    int fd = 0;
    while( (fd<MAX_OPEN_FILES)&&(current->files[fd])!=NULL )
      fd++;
    
    //IF NO FILE DESCRIPTORS REMAIN
    if( fd == MAX_OPEN_FILES )
      return -EOTHERS;
    
    //HERE WE HAVE FOUND A VALID FILE DESCRIPTOR
    current->files[fd] = current->files[oldfd];

    //UPDATING THE REFERENCE COUNT
    current->files[fd]->ref_count = current->files[fd]->ref_count + 1; 
    return fd;
    
}


int fd_dup2(struct exec_context *current, int oldfd, int newfd)
{
  /** TODO Implementation of the dup2 
    *  Read the man page of dup2 and implement accordingly 
    *  return the file descriptor,
    *  Incase of Error return valid Error code 
    * */
    int ret_fd = -EINVAL;

    if(current == NULL)
      return -EINVAL;

    if((oldfd>=MAX_OPEN_FILES)||oldfd<0)
      return -EINVAL;

    if((newfd>=MAX_OPEN_FILES)||newfd<0)
      return -EINVAL;

    if(oldfd==newfd)//What to return in this case is unclear as they both might be closed.
      return newfd;

    //Check if oldfd is an open file descriptor
    if( (current->files[oldfd])==NULL )
      return -EOTHERS;//DISCUSS WITH SOMEONE
    
    if( current->files[newfd] == NULL )//no need to close it
    {
      current->files[newfd] = current->files[oldfd];
      current->files[newfd]->ref_count = current->files[newfd]->ref_count+1; //refcount updated here as well
      return newfd;
    }
    else//newfd was open earlier,we have to close and reuse it
    {
      int ret_gen_close = generic_close( current->files[newfd] );
      if( ret_gen_close!=0 )//close fails
        return -EOTHERS;
      else//closed successfully
      {
        current->files[newfd] = current->files[oldfd];//tHIS IS THE ERROR,FILE REF IS NOT INCREMENTED
        current->files[newfd]->ref_count = current->files[newfd]->ref_count+1; 
        return newfd;
      }
      
      
    }
    
    return ret_fd;
}
