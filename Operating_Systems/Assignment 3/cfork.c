#include <cfork.h>
#include <page.h>
#include <mmap.h>


struct vm_area* find_where_is_add_exclusive(struct vm_area *head, u64 addr)
{
    while(head!=NULL)
    {
        if((head->vm_start<=addr)&&(addr<head->vm_end))//removed equal to as in this case i AM NOT decrementing the end values
            return head;
        head = head->vm_next;
    }
    return NULL;
}

/* You need to implement cfork_copy_mm which will be called from do_cfork in entry.c. Don't remove copy_os_pts()*/
//make page tables for the child proceess,assign a vm area,flush the tlb as it will have the entries for the old one, as acid wont be there and cow fault needs to be handled
void cfork_copy_mm(struct exec_context *child, struct exec_context *parent ){


    void *os_addr;
    u64 vaddr; 
    struct mm_segment *seg;
    child->pgd = os_pfn_alloc(OS_PT_REG);

    os_addr = osmap(child->pgd);
    bzero((char *)os_addr, PAGE_SIZE);
    //CODE segment
    seg = &parent->mms[MM_SEG_CODE];
    for(vaddr = seg->start; vaddr < seg->next_free; vaddr += PAGE_SIZE){
        u64 *parent_pte =  get_user_pte(parent, vaddr, 0);
        if(parent_pte)
             install_ptable((u64) os_addr, seg, vaddr, (*parent_pte & FLAG_MASK) >> PAGE_SHIFT);   
    } 
    //RODATA segment
   
    seg = &parent->mms[MM_SEG_RODATA];
    for(vaddr = seg->start; vaddr < seg->next_free; vaddr += PAGE_SIZE){
        u64 *parent_pte =  get_user_pte(parent, vaddr, 0);
        if(parent_pte)
             install_ptable((u64)os_addr, seg, vaddr, (*parent_pte & FLAG_MASK) >> PAGE_SHIFT);   
    }

    //DATA segment: DOES NOT CONTAIN THE VM_AREA
  seg = &parent->mms[MM_SEG_DATA];
  for(vaddr = seg->start; vaddr < seg->next_free; vaddr += PAGE_SIZE){
      u64 *parent_pte =  get_user_pte(parent, vaddr, 0);
      
      if(parent_pte){
          // map_physical_page((u64)os_addr, vaddr, (u32)(0), (*parent_pte & FLAG_MASK) >> PAGE_SHIFT);//setting the child thing
          // map_physical_page(osmap(parent->pgd), vaddr, (u32)(0), (*parent_pte & FLAG_MASK) >> PAGE_SHIFT);//setting the parent thing
          // install_ptable((u64)os_addr, seg, vaddr, (*parent_pte & FLAG_MASK) >> PAGE_SHIFT);   //WHAT DO WE HAVE TO DO OUT OF THE 2
          //changed the pte in the parents table
        //changes the physical page whrite bit ti readonly and increments the ref_count
        increment_pfn_info_refcount(get_pfn_info(map_physical_page((u64)osmap(parent->pgd), vaddr, (u32)(0), (*parent_pte & FLAG_MASK) >> PAGE_SHIFT)));
        //changes the pte in the chiulds table
        map_physical_page((u64)osmap(child->pgd), vaddr, (u32)(0), (*parent_pte & FLAG_MASK) >> PAGE_SHIFT);

        u64 *parent_pte =  get_user_pte(parent, vaddr, 0);
        //FLUSH THE TLB
        asm volatile ("invlpg (%0);" 
                    :: "r"(vaddr) //SEE THAT THE FLUSHING IS CORRECT
                    : "memory");   // Flush TLB
      }

      
  } 

  
  //STACK segment
  seg = &parent->mms[MM_SEG_STACK];
  for(vaddr = seg->end - PAGE_SIZE; vaddr >= seg->next_free; vaddr -= PAGE_SIZE){
      u64 *parent_pte =  get_user_pte(parent, vaddr, 0);
      
     if(parent_pte){
           u64 pfn = install_ptable((u64)os_addr, seg, vaddr, 0);  //Returns the blank page  
           pfn = (u64)osmap(pfn);
           memcpy((char *)pfn, (char *)(*parent_pte & FLAG_MASK), PAGE_SIZE); 
      }
  }


  // MMAP_AREA segment
  struct vm_area* head = parent->vm_area;
  while(head!=NULL)
  {
    for (unsigned long i = head->vm_start; i <  head->vm_end ; i+=4096)
    {
      //get the page and change it to read only

      u64 *parent_pte =  get_user_pte(parent, i, 0);
      if(parent_pte)//we get to the page and changed its entry to read only, It might be the case that evenn after existing in th evm_area, the page might not be mapped
      {
        //changed the pte in the parents table
        //changes the physical page whrite bit ti readonly and increments the ref_count
        increment_pfn_info_refcount(get_pfn_info(map_physical_page((u64)osmap(parent->pgd), i, (u32)(0), (*parent_pte & FLAG_MASK) >> PAGE_SHIFT)));
        //changes the pte in the chiulds table
        map_physical_page((u64)osmap(child->pgd), i, (u32)(0), (*parent_pte & FLAG_MASK) >> PAGE_SHIFT);

        u64 *parent_pte =  get_user_pte(parent, i, 0);
        //FLUSH THE TLB
        asm volatile ("invlpg (%0);" 
                    :: "r"(i) //SEE THAT THE FLUSHING IS CORRECT
                    : "memory");   // Flush TLB
      }
             
            
    }
      
    head = head->vm_next;
  }


  //FLUSH THE TLB

    //copy the list from the child in to the paren
    //Count the numer of nodes
    head = parent->vm_area;
    int count = 0;
    while(head!=NULL)
    {
     head = head->vm_next;
     count++;
    }
    //assigning node according to count
    if(count == 0)
    {
      child->vm_area = NULL;
    }
    else
    {
      child->vm_area = alloc_vm_area();
      struct vm_area* curr_node = child->vm_area;
      for(int i = 0;i<count-1;i++)
      {
        curr_node->vm_next = alloc_vm_area();
        curr_node = curr_node->vm_next;
      }
    }
    //Copying the fields
    head = parent->vm_area;
    struct vm_area* child_head = child->vm_area;
    while(head!=NULL)
    {
      // child_head->vm_next = head->vm_next;//wrong, you know why
      child_head->access_flags = head->access_flags;
      child_head->vm_end = head->vm_end;
      child_head->vm_start = head->vm_start;
      head = head->vm_next;
      child_head = child_head->vm_next;
    }
    
    copy_os_pts(parent->pgd, child->pgd); 




    return;
    
}

/* You need to implement cfork_copy_mm which will be called from do_vfork in entry.c.*/
void vfork_copy_mm(struct exec_context *child, struct exec_context *parent ){
  
    void *os_addr;
    u64 vaddr; 
    struct mm_segment *seg;

    child->pgd = parent->pgd;
    parent->state = WAITING;

    os_addr = osmap(child->pgd);
  
  //STACK segment
  long num_pages = 0;
  seg = &parent->mms[MM_SEG_STACK];
  for(vaddr = seg->end - PAGE_SIZE; vaddr >= seg->next_free; vaddr -= PAGE_SIZE){
      num_pages ++;
  }

  // get_user_pte(child,MMAP_AREA_START,1);
  // get_user_pte(parent,MMAP_AREA_START,1);

  for(vaddr = seg->end - PAGE_SIZE; vaddr >= seg->next_free; vaddr -= PAGE_SIZE){
      u64 *parent_pte =  get_user_pte(parent, vaddr, 0);
      
     if(parent_pte){
          // u64 pfn = install_ptable((u64)os_addr, seg, vaddr-num_pages*PAGE_SIZE, 0);  //Returns the blank page
          // //  if(get_pfn_info_refcount(get_pfn_info(pfn)) == 0)
          // //  {
          //    pfn = (u64)osmap(pfn);//change here
          // //  }
          // memcpy((char *)pfn, (char *)(*parent_pte & FLAG_MASK), PAGE_SIZE); 
          u64 pfn;
          u64 *prev_stack_pte = get_user_pte(parent, vaddr-num_pages*PAGE_SIZE,0);
          if(!*prev_stack_pte)//the pte entry does not exist,create one
          {
            pfn = install_ptable((u64)os_addr, seg, vaddr-num_pages*PAGE_SIZE, 0);
          }
          else
          {
            pfn  = (*prev_stack_pte & FLAG_MASK) >> PAGE_SHIFT;
            
          }
          pfn = (u64)osmap(pfn);
          memcpy((char *)pfn, (char *)(*parent_pte & FLAG_MASK), PAGE_SIZE);//copy the memory
          
      }
  }
  child->regs.entry_rsp-=num_pages*PAGE_SIZE;
  child->regs.rbp-=num_pages*PAGE_SIZE;
  struct mm_segment *seg_stack_child = &child->mms[MM_SEG_STACK];
  seg_stack_child->next_free = seg->next_free-num_pages*PAGE_SIZE;



  // MMAP_AREA segment

  //FLUSH THE TLB
    
    return;
    
}

/*You need to implement handle_cow_fault which will be called from do_page_fault 
incase of a copy-on-write fault

* For valid acess. Map the physical page 
 * Return 1
 * 
 * For invalid access,
 * Return -1. 
*/

int handle_cow_fault(struct exec_context *current, u64 cr2){
    //Check the range,check the permissions, check the ref count, and update accordingly
    ///NOTE that I am currently checking the range only in the vm_area segment, ask if we need to check elsewhere as well.

    if(cr2 >= MMAP_AREA_START && cr2 <= MMAP_AREA_END)
    {
      if((find_where_is_add_exclusive(current->vm_area,cr2)!=NULL)&&(find_where_is_add_exclusive(current->vm_area,cr2)->access_flags & PROT_WRITE))
      {
        u64 *old_pte =  get_user_pte(current, cr2, 0);//A POINTER TO THE PTE
        u32 old_pfn = (u32)((*old_pte & FLAG_MASK) >> PAGE_SHIFT);//VALUE OF THE PFN(THE OLD ONE)
        //Since the error code is such the page will be mapped(not only the vm_area)
        u8 refcount = get_pfn_info_refcount(get_pfn_info(old_pfn));
        if(refcount>(u8)1)
        {

          //refcount>1,we need to map a new page and copy the old data in it and then decrement the refcountn of the new page
          u32 new_pfn = map_physical_page((u64)osmap(current->pgd), cr2, (u32)(2), 0);  //Returns the blank page, and set a writie bit to it
          //HENCEFORTH OLD_PFN CHANGES!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
          u64 new_address = (u64)osmap(new_pfn);//AGAIN SHIFT IS LEFT BY 12 BITS TO GET ADDRESS AND NOT PFN
          u64 old_address = (u64)osmap(old_pfn);//AGAIN SHIFT IS LEFT BY 12 BITS TO GET ADDRESS AND NOT PFN
          memcpy((char *)new_address, (char *)(old_address), PAGE_SIZE);//cpoy the page
          //increment new pfn refcount!!!!!!!!DONT NEED TO ALREADY DONE IN set_pfn_info.
          //decrement old 
          decrement_pfn_info_refcount(get_pfn_info(old_pfn));


          //FLUSH THE TLB
        asm volatile ("invlpg (%0);" 
                    :: "r"(cr2) //SEE THAT THE FLUSHING IS CORRECT
                    : "memory");   // Flush TLB


        }
        else//refcount = 1,just changing the permissions and flushing tlb is sufficient
        {
          //change permission
          map_physical_page((u64)osmap(current->pgd), cr2, (u32)(2), (*old_pte & FLAG_MASK) >> PAGE_SHIFT);
          //FLUSH THE TLB
          asm volatile ("invlpg (%0);" 
                      :: "r"(cr2) //SEE THAT THE FLUSHING IS CORRECT
                      : "memory");   // Flush TLB
        }

      }
      
      else
      {
        return -1;
      }
      

    }
    else if((cr2>=((&current->mms[MM_SEG_DATA])->start)) && (cr2< ((&current->mms[MM_SEG_DATA])->next_free)))
    {
        u64 *old_pte =  get_user_pte(current, cr2, 0);//A POINTER TO THE PTE
        u32 old_pfn = (u32)((*old_pte & FLAG_MASK) >> PAGE_SHIFT);
        u8 refcount = get_pfn_info_refcount(get_pfn_info(old_pfn));
        if(refcount>(u8)1)
        {
          u32 new_pfn = map_physical_page((u64)osmap(current->pgd), cr2, (u32)(2), 0);  //Returns the blank page, and set a writie bit to it
          u64 new_address = (u64)osmap(new_pfn);//AGAIN SHIFT IS LEFT BY 12 BITS TO GET ADDRESS AND NOT PFN
          u64 old_address = (u64)osmap(old_pfn);//AGAIN SHIFT IS LEFT BY 12 BITS TO GET ADDRESS AND NOT PFN
          memcpy((char *)new_address, (char *)(old_address), PAGE_SIZE);//cpoy the page
          decrement_pfn_info_refcount(get_pfn_info(old_pfn));
          //FLUSH THE TLB
          asm volatile ("invlpg (%0);" 
                      :: "r"(cr2) //SEE THAT THE FLUSHING IS CORRECT
                      : "memory");   // Flush TLB
        }
        else
        {
          map_physical_page((u64)osmap(current->pgd), cr2, (u32)(2), (*old_pte & FLAG_MASK) >> PAGE_SHIFT);
          //FLUSH THE TLB
          asm volatile ("invlpg (%0);" 
                      :: "r"(cr2) //SEE THAT THE FLUSHING IS CORRECT
                      : "memory");   // Flush TLB          
        }
        
    }
    else
    {
      return -1;
    }
    
    return 1;
}

/* You need to handle any specific exit case for vfork here, called from do_exit*/
void vfork_exit_handle(struct exec_context *ctx){

  u32 parent_proc_id = ctx->ppid;
  struct exec_context *parent = get_ctx_by_pid(parent_proc_id);
  // parent->state = READY;
  if((parent!=NULL)&&(parent->pgd == (ctx->pgd)))
  {
    parent->state = READY;
    parent->vm_area = ctx->vm_area;
    parent->mms[MM_SEG_DATA].next_free = ctx->mms[MM_SEG_DATA].next_free;
  }
  else
  {
    ;
  }
  

    return;
}