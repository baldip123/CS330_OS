#include<types.h>
#include<mmap.h>
#include<page.h>

/**
 * Handling Physical Pages
 * on the basis of the return values do the following in the following functions:
 * In MMAP:
 *      IF MPOPULATE: Do the following: Assign a physical page using a while loop to every page in the address range
 * In Munmap
 *      Flush the tlb for safety and use user_unmap on every page which is assigned(dont worry about it he unmap function takes care of it)
 *      and is being unmapped
 * In Mprotect
 *      For every page in the range change the permission in the PTE ENTRY, and flush the tlb afterwards
 * 
 * 
 * In handle _page_faut see it appropriately.
 * Also flush the tlb if needed.
 * */

int count_num_vm_area(struct vm_area*head)
{
    int count = 0;
    while(head!=NULL)
    {
        count++;
        head=head->vm_next;
    }
    return count;
}

int can_new_be_merged_with_prev(struct vm_area* prev_node,long start_address,int length, int prot)
{
    if(prev_node == NULL)
        return 0;
    
    if(((prev_node->vm_end +1) == start_address) && (prev_node->access_flags == prot))
        return 1;

    return 0;
}

int can_new_be_merged_with_next(struct vm_area* next_node,long start_address,int length, int prot)
{
    if(next_node == NULL)
        return 0;
    
    if(((next_node->vm_start) == (start_address + ((long)length))) && (next_node->access_flags == prot))
        return 1;
    
    return 0;
}

void add_inc(struct vm_area* head)
{
    while (head!=NULL)
    {
        head->vm_end++;
        head = head->vm_next;
    }
    
}
void add_dec(struct vm_area* head)
{
    while (head!=NULL)
    {
        head->vm_end--;
        head = head->vm_next;
    }
    
}
/**
 * Function will invoked whenever there is page fault. (Lazy allocation)
 * 
 * For valid acess. Map the physical page 
 * Return 1
 * 
 * For invalid access,
 * Return -1. 
 */

struct vm_area* find_where_is_add(struct vm_area *head, u64 addr)
{
    while(head!=NULL)
    {
        if((head->vm_start<=addr)&&(addr<=head->vm_end))
            return head;
        head = head->vm_next;
    }
    return NULL;
}

int vm_area_pagefault(struct exec_context *current, u64 addr, int error_code)
{
    /**
     * PSEUDOCODE:
     * 1.Find which PFN is the accessed adress a part of.
     * DOUBT, SEE WHICH CASE LEAD TOA N ERROR 
     * CASES:
     *      a.If its not a part of the vm_area, then
     *      b.If its a part of the vm_area and the PFN IS NOT SET
     * 2.Take care of the other erro code which arises when it is an issue of whether something is assigned or not
    */
    
    int fault_fixed = -1;
    add_dec(current->vm_area);//to dec the end end
    if(error_code == 6)//could be because the page is not assigned or the write permission is not there
    {
        if((find_where_is_add(current->vm_area, addr))&&((find_where_is_add(current->vm_area, addr)->access_flags & PROT_WRITE)))//WRITE IS NOT THERE.
        {
            map_physical_page((unsigned long)osmap(current->pgd), addr, MM_WR, 0);
            fault_fixed = 1;
        } 
        else//permission is not write or vm_area is not there
        {
            map_physical_page((unsigned long)osmap(current->pgd), addr, MM_WR, 0);
            fault_fixed = -1;
        }
        
    }
    else if(error_code == 4)//read access, means definitely we need to mamp a page
    {
        if(!(find_where_is_add(current->vm_area, addr)))
        {
            fault_fixed = -1;
        }
            //return is either th eaddress is nowhere to be found 
        else//write is there map a physical page
        {
            map_physical_page((unsigned long)osmap(current->pgd), addr, find_where_is_add(current->vm_area, addr)->access_flags, 0);
            fault_fixed = 1;
        }

    }

    add_inc(current->vm_area);
    return fault_fixed;
}




/**
 * mprotect System call Implementation.
 */

int get_page_aligned_length(int length)
{
    if(length <= 0)
    {
        return -1;
    }
    else if(length % (PAGE_SIZE) == 0)
    {
        return length;
    }
    else
    {
        int residue = length % PAGE_SIZE;
        residue = PAGE_SIZE - residue;
        length = length + residue;
        return length;
    }
    
}


int check_if_range_contigous(struct vm_area* head, struct vm_area* tail)
{
    while(head!=tail)
    {
        if((head->vm_end+1)!=head->vm_next->vm_start)
            return 0;
        head = head->vm_next;
    }
    return 1;
}

int num_area_spanned( struct vm_area* head, struct vm_area* tail )
{
    int span = 1;
    while(head!=tail)
    {
        head = head->vm_next;
        span++;
    }
    return span;
}


int check_kind_of_overlap(struct vm_area* node, u64 addr, int length)
{
    //assumtion length is positive
    //check that typecasting does not lead to problems
    //inclusive end_range
    u64 end_range = addr+((u64)length)-1;


    if(node == NULL)
        return -1;
    //Partial overlap from beginning
    else if((node->vm_start>=addr)&&(node->vm_end>end_range)&&(node->vm_start<end_range))
        return 1;
    //Partial overlap till end
    else if((node->vm_start<addr)&&(node->vm_end>addr)&&(node->vm_end<=end_range))
        return 2;
    //Partial overlap in middle
    else if((node->vm_start<addr)&&(node->vm_end>end_range))
        return 3;
    //complete overlap
    else if((node->vm_start>=addr)&&(node->vm_end<=end_range))
        return 4;
    // no overlap
    else
        return -1;

}

struct vm_area* merge_with_next_if_required_with_return(struct vm_area*  curr_node)
{
    if(curr_node==NULL)
        return NULL;
    
    if(curr_node -> vm_next ==NULL)
        return NULL;

    if((curr_node->access_flags == curr_node->vm_next->access_flags)&&(curr_node->vm_end+1 == curr_node->vm_next->vm_start))
    {//merge the two
        struct vm_area* delete_node = curr_node->vm_next;
        curr_node->vm_end = curr_node->vm_next->vm_end;
        curr_node->vm_next = curr_node->vm_next->vm_next;
        dealloc_vm_area(delete_node);
        return curr_node;
    }

    return NULL;//not merged
    
}
struct vm_area* get_prev_node(struct exec_context *current, struct vm_area* node_to_be_deleted)
{
    struct vm_area* temp = current->vm_area, *prev;
    // If head node itself holds the key to be deleted
    if (temp != NULL && (temp == node_to_be_deleted)) 
    { 
        return NULL;//as no prev node 
    } 
  
    // Search for the key to be deleted, keep track of the 
    // previous node as we need to change 'prev->next' 
    while (temp != NULL && (temp != node_to_be_deleted)) 
    { 
        prev = temp; 
        temp = temp->vm_next; 
    } 
  
    // If key was not present in linked list 
    if (temp == NULL) return NULL; 
  
    // Unlink the node from linked list 
    return prev;
}

void  merge_starting_from_head(struct vm_area*head)
{
    int count = 0;
    while(head!=NULL)
    {
        count++;
        if(merge_with_next_if_required_with_return(head) == NULL)//NOT MERGED
            head=head->vm_next;
        //If merged you dont need to do next
    }
}


int vm_area_mprotect(struct exec_context *current, u64 addr, int length, int prot)
{
    
    int isValid = -1;
    add_dec(current->vm_area);
    /**Code Idea
     * 1.Check if its contiguos in the range or not
     * 2.if not contiguous, false return
     * 3.If conntigous, keep finding kind of overlap and doing appropriate operation
     * 4.After start merging from left(the head) 
     */
    if(length<=0)
    {
        add_inc(current->vm_area);
        return -1;//NOT FOUND ADD.
    }

    length = get_page_aligned_length(length);

    struct vm_area* head = find_where_is_add(current->vm_area,addr);
    struct vm_area* tail = find_where_is_add(current->vm_area,addr+length-1);
    if(head == NULL || tail == NULL)
    {
        add_inc(current->vm_area);
        return -1;//NOT FOUND ADD.
    }    
    //1.CONTIGOUS CHECK
    if(!check_if_range_contigous(head,tail))
    {
        add_inc(current->vm_area);
        return -1;//NOT CONTIGOUS
    }



//FOR CHECKING THE NUMBER OF VM AREAS

    if(num_area_spanned(head,tail) >= 3)
        ;
    else if(num_area_spanned(head,tail) ==  1)
    {
        if(check_kind_of_overlap(head,addr,length) == 4)
            ;
        else if(check_kind_of_overlap(head,addr,length) == 3)
        {
            if (head->access_flags == prot)
            {
                ;//its still ok as no change will be done;
            }
            else if(count_num_vm_area(current->vm_area) == 127)
            {
                add_inc(current->vm_area);
                return -1;//more vm area
            }
            
        }
        else if(check_kind_of_overlap(head,addr,length) == 2)
        {
            if( can_new_be_merged_with_next(head->vm_next,addr,length,prot)||head->access_flags == prot)
            {
                ;//It can be merged with next, or with the same one after splitting
            }
            else
            {
                if (count_num_vm_area(current->vm_area) == 128)
                {
                    add_inc(current->vm_area);
                    return -1;//more vm area
                }
            }
            
        }
        else//kind of overlap is 1
        {
            if((get_prev_node(current,head) == NULL))
            {
                if(head->access_flags!=prot && count_num_vm_area(current->vm_area) == 128)
                {
                    add_inc(current->vm_area);
                    return -1;//more vm area                
                }
            }
            else
            {
                if(can_new_be_merged_with_prev(get_prev_node(current,head),head->vm_start,length,prot))
                ;
                else
                {
                    if(head->access_flags!=prot && count_num_vm_area(current->vm_area) == 128)
                    {
                        add_inc(current->vm_area);
                        return -1;//more vm area                
                    }
                }
                
            }
            
        }
        
    }
    else if(num_area_spanned(head,tail) ==  2)
    {
        if((check_kind_of_overlap(head,addr,length) == 4) || (check_kind_of_overlap(tail,addr,length) == 4))
            ;//complete overlap on either os the two
        else//partial overlap on both
        {
            if(head->access_flags == prot || tail->access_flags == prot)//either has the same permissions as the prot
            ;
            else if(count_num_vm_area(current->vm_area) ==128 )
            {
                add_inc(current->vm_area);
                return -1;//more vm area
            }
            
        }
    }
    






    //----------------------------------------------------------------------------------------------------------------------------
    //3.TYPE OF OVERLAP AND APPROPRIATE OPERATION
    struct vm_area* curr_node = head;
    while((curr_node!=NULL)&&(curr_node->vm_start<=addr+length-1))
    {
        //Perform the staps of the Pseudocode
        int type_of_overlap = check_kind_of_overlap(curr_node,addr,length);
        if(type_of_overlap == 1)
        {
            struct vm_area* next = curr_node->vm_next;
            long end_bef_del = curr_node->vm_end;
            curr_node->vm_end = (long)addr+length-1;
            curr_node->vm_next = alloc_vm_area();
            if(curr_node->vm_next == NULL)
            {
                add_inc(current->vm_area);
                return -EINVAL;
            }
            curr_node->vm_next->vm_start = (long)addr+length;
            curr_node->vm_next->vm_end = end_bef_del;
            //NOT YET CHANGED THE ACCESS FLAGS OF THE CURR NODE
            curr_node->vm_next->access_flags = curr_node->access_flags;
            curr_node->vm_next->vm_next = next;
            //CHANGE ACCESS PERMISSIONS OF THE CURRENT NODE
            curr_node->access_flags = prot;
        }
        else if(type_of_overlap == 2)
        {
            struct vm_area* next = curr_node->vm_next;
            long end_bef_del = curr_node->vm_end;
            curr_node->vm_end = (long)addr-1;
            curr_node->vm_next = alloc_vm_area();
            if(curr_node->vm_next == NULL)
            {
                add_inc(current->vm_area);
                return -EINVAL;
            }
            curr_node->vm_next->vm_start = (long)addr;
            curr_node->vm_next->vm_end = end_bef_del;
            //CHANGE PERMISSIONS FOR THE SECOND NODE
            curr_node->vm_next->access_flags = prot;
            curr_node->vm_next->vm_next = next;
        }
        else if(type_of_overlap == 3)
        {
            //Partial overlap in middle handle appropriately
            //split the node into 2 as follows
            struct vm_area* next = curr_node->vm_next;
            long end_bef_del = curr_node->vm_end;
            //THE FIRST NODE
            // start does not change
            curr_node->vm_end = (long)addr-1;
            //access does not change
            curr_node->vm_next = alloc_vm_area();
            //
            if(curr_node->vm_next == NULL)
            {
                add_inc(current->vm_area);
                return -EINVAL;
            }
            //THE SECOND NODE
            curr_node->vm_next->vm_start = (long)addr;
            curr_node->vm_next->vm_end = (long)addr+length-1;
            curr_node->vm_next->access_flags = prot;
            curr_node->vm_next->vm_next = alloc_vm_area();
            //
            //
            if(curr_node->vm_next->vm_next == NULL)
            {
                add_inc(current->vm_area);
                return -EINVAL;
            }
            //THE THIRD NODE
            curr_node->vm_next->vm_next->vm_start = (long)addr+length;
            curr_node->vm_next->vm_next->vm_end = end_bef_del;
            curr_node->vm_next->vm_next->access_flags = curr_node->access_flags;
            curr_node->vm_next->vm_next->vm_next = next;
            //

                
        }
        else if(type_of_overlap == 4)//Complete overlap//handle deletion
        {
            curr_node->access_flags = prot;
        }
        //Go to the next node.
        curr_node = curr_node->vm_next;
    }
    //-------------------------------------------------------------------------------------------------------------------
    //4.MERGE STARTING FROM HEAD
    // struct vm_area* curr = current->vm_area;
    // while(curr!=NULL)
    // {
    //     curr = curr->vm_next;
    // }


    merge_starting_from_head(current->vm_area);

    add_inc(current->vm_area);

    int i = 0;
    for(i = 0;i<length;i+=PAGE_SIZE)//not inclusive as we are talking about the starting length.
    {
        u64 *pte_entry = get_user_pte(current, addr+(u64)i, 0);
        if(!pte_entry)
            continue;//as the pte is not there and I cant changer permissions
            // return -1;//I dont think this needs to be there as we can change permission even is PTE IS not assigned

        // if(prot & MM_WR)
        //     *pte_entry |= 0x2;//this will correctly set it to write
        // else
        //     map_physical_page((u64)osmap(current->pgd), addr+i, (u32)(0), (*pte_entry & FLAG_MASK) >> PAGE_SHIFT);
        //     // *pte_entry |= 0x0;//this will not correctly set to read as it is an or
        else
        {
            if(get_pfn_info_refcount(get_pfn_info((*pte_entry & FLAG_MASK) >> PAGE_SHIFT)) == 1)
            {
                if(prot & MM_WR)
                    *pte_entry |= 0x2;//this will correctly set it to write
                else
                    map_physical_page((u64)osmap(current->pgd), addr+i, (u32)(0), (*pte_entry & FLAG_MASK) >> PAGE_SHIFT);
            }
            else//refcount is more than 1, and is new permissons is read, it will already be read, if it is write,it will be a cow fault.
            {
                ;
            }
            
        }
        
        u64 ADD_FLUSH = addr+i;
            //flush the tlb
        asm volatile ("invlpg (%0);" 
                    :: "r"(ADD_FLUSH) //SEE THAT THE FLUSHING IS CORRECT
                    : "memory");   // Flush TLB
    }
    return 0;
}

//Gives A pointer  to the element before the new mapping,if no mapping is possible returns NULL, uisng the pointer you can find wheter to merge the two mappings or not
int find_new_mapping(struct vm_area* head,int length)
{
    int pos = 1;//to return the position where the new_node is to be inserted, starts at 1.
    long start_add = MMAP_AREA_START;
    if(length+MMAP_AREA_START-1>=MMAP_AREA_END)
        return -EINVAL;

    while(head!=NULL)
    {
        if(start_add+length-1>=head->vm_start)
        {
            pos++;
            if (head->vm_next == NULL)
            {
                if(head->vm_end+1+length-1>=MMAP_AREA_END)
                {
                    return -EINVAL;
                }
                else
                {
                    return pos;
                }
                
            }
            start_add = head->vm_end+1;
            
        }
        else
        {
            return pos;
        }
        head = head->vm_next;
    }
    return pos;
}

int find_new_mapping_after(struct vm_area* head,int length,int pos_min)
{
    int pos = 1;//to return the position where the new_node is to be inserted, starts at 1.
    long start_add = MMAP_AREA_START;

    if(length+MMAP_AREA_START-1>=MMAP_AREA_END)
        return -EINVAL;

    while(head!=NULL)
    {
        if(start_add+length-1>=head->vm_start)
        {
            pos++;
            if (head->vm_next == NULL)
            {
                if(head->vm_end+1+length-1>=MMAP_AREA_END)
                {
                    return -EINVAL;
                }
                else
                {
                    if(pos>pos_min)
                        return pos;
                }
                
            }
            start_add = head->vm_end+1;
            
        }
        else
        {
            if(pos>pos_min)
                return pos;
        }
        head = head->vm_next;
    }
    if(pos>pos_min)
        return pos;
    else
        return -EINVAL;
}

struct vm_area* get_node_at_pos(struct vm_area* head,int pos)
{
    if(head == NULL)
        return NULL;
    else 
    {
        for(int i= 1;i<pos;i++)
        {
            head = head->vm_next;
        }
        return head;
    }
}


void merge_with_next_if_required(struct vm_area*  curr_node)
{
    if(curr_node==NULL)
        return;
    
    if(curr_node -> vm_next ==NULL)
        return;

    if((curr_node->access_flags == curr_node->vm_next->access_flags)&&(curr_node->vm_end+1 == curr_node->vm_next->vm_start))
    {//merge the two
        struct vm_area* delete_node = curr_node->vm_next;
        curr_node->vm_end = curr_node->vm_next->vm_end;
        curr_node->vm_next = curr_node->vm_next->vm_next;
        dealloc_vm_area(delete_node);
        return;
    }
    
}

int is_address_available_in_list(struct vm_area* head, long start_address, int length)
{
    /*sufficient and necc conditions
    1.MMAP_END>address>=MMAP_START
    2.FOR_ANY_LIST_ELEMENT not(vm_end>=address>=vm_start)
    */
   if( !((start_address>=MMAP_AREA_START)&&(start_address+length-1<=MMAP_AREA_END)) )//The address definitely lies in the MMAP RANGE
   {
       return -1;
   }
   else
   {//check that it does not overlap with any interval,or return the position of the interval that it overlaps with
       int pos = 1;
       while (head!=NULL)
       {
           if( (head->vm_end<start_address)||(head->vm_start>start_address+length-1)  )
           {
               pos++;
               head = head->vm_next;
           }
           else
           {
               return pos;//overlap
           }
           
       }
       return 0;
       
   }
}

int get_insertion_position(struct vm_area* head, long start_address)
{
    int pos = 1;
    while(head!=NULL && (head->vm_start < start_address))
    {
        head = head->vm_next;
        pos++;
    }
    return pos;
}

/**
 * mmap system call implementation.
 */





long vm_area_map(struct exec_context *current, u64 addr, int length, int prot, int flags)
{
    //!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
    //ADD THE THING ABOUT MAP FIXED
    //!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
    add_dec(current->vm_area);
    long add_to_return;
    //1.VALIDATE AND GET A PAFE ALIGNED LENGTH
    length = get_page_aligned_length(length);
    if(length == -1)
    {
        add_inc(current->vm_area);
        return -EINVAL;
    }
//-------------------------------------------------------------------------------------------------------------------------
    //2.CASE 1) IF HINT ADDRESS IS NOT ASSIGNED,
    //I HAVE NOT RETURNED ANYTHING,FIND OUT WHAT TO RETURN
    if(addr == (u64)NULL)//without hint address
    {
        int position_to_insert = find_new_mapping(current->vm_area,length);
        if(position_to_insert == 1)//to be inserted at the head
        {
            struct vm_area* next_node = current->vm_area;
            if(can_new_be_merged_with_next(next_node,MMAP_AREA_START,length,prot))
            {
                //I assume that the length was previously 128 or less only, and it cant increase now as it will be merged
                next_node -> vm_start = MMAP_AREA_START;
                add_to_return = MMAP_AREA_START;
            }
            else
            {
                if(count_num_vm_area(current->vm_area) == 128)
                {
                    add_to_return = -1;
                }
                else
                {
                    current->vm_area = alloc_vm_area();
                    current->vm_area->vm_start = MMAP_AREA_START;
                    current->vm_area->vm_end = MMAP_AREA_START+length-1;
                    current->vm_area->vm_next = next_node;
                    current->vm_area->access_flags = prot;
                    add_to_return = MMAP_AREA_START;
                }
            }
            

        }
        else if (position_to_insert == -EINVAL)
        {
            add_to_return = -EINVAL;
        }
        
        else
        {//not to be inserted at the head inserted at the previous node
            struct vm_area* node_after_which_inserted = get_node_at_pos(current->vm_area,position_to_insert-1);
            if(node_after_which_inserted == NULL)
            {
                add_to_return = -EINVAL;
            }
            else
            {
                struct vm_area* next_node = node_after_which_inserted->vm_next;
                int next_merge = can_new_be_merged_with_next(next_node,node_after_which_inserted->vm_end+1,length,prot);;
                int prev_merge = can_new_be_merged_with_prev(node_after_which_inserted,node_after_which_inserted->vm_end+1,length,prot);
                long return_add = node_after_which_inserted->vm_end+1;
                if(prev_merge && next_merge)
                {
                    //number will go down only
                    struct vm_area* next_next_node = next_node->vm_next;
                    node_after_which_inserted->vm_end = next_node->vm_end;
                    node_after_which_inserted->vm_next = next_next_node;
                    dealloc_vm_area(next_node);
                    add_to_return = return_add;
                }
                else if(prev_merge)
                {
                    node_after_which_inserted->vm_end += length;
                    add_to_return = return_add;
                }
                else if(next_merge)
                {
                    next_node->vm_start = node_after_which_inserted->vm_end+1;
                    add_to_return = return_add;
                }
                else//no merge
                {
                    if(count_num_vm_area(current->vm_area) == 128)
                    {
                        add_to_return = -1;
                    }
                    else
                    {
                        node_after_which_inserted->vm_next = alloc_vm_area();
                        node_after_which_inserted->vm_next->access_flags = prot;
                        node_after_which_inserted->vm_next->vm_start = return_add;
                        node_after_which_inserted->vm_next->vm_next = next_node;
                        node_after_which_inserted->vm_next->vm_end = return_add+length-1;
                        add_to_return = return_add;
                    }
                }
            }
        }
    }
//-------------------------------------------------------------------------------------------------------------------------
    //3.CASE 2) IF HINT ADDRESS IS ASSIGNED,FIND OUT WHAT TO RETURN
    else
    {


        //check if the address is available
        int availability = is_address_available_in_list(current->vm_area,(long)addr, length);
        //4.CASE 1) IF HINT ADDRESS IS ASSIGNED AVAILABLE,FIND WHAT TO RETURN
        if(availability == 0)//the address is available
        {
            int position_to_insert = get_insertion_position(current->vm_area, (long)addr);

            if(position_to_insert == 1)//to be inserted at the head
            {
                struct vm_area* next_node = current->vm_area;
                if(can_new_be_merged_with_next(next_node,addr,length,prot))
                {
                    //I assume that the length was previously 128 or less only, and it cant increase now as it will be merged
                    next_node -> vm_start = addr;
                    add_to_return = addr;
                }
                else
                {
                    if(count_num_vm_area(current->vm_area) == 128)
                    {
                        add_to_return = -1;
                    }
                    else
                    {
                        current->vm_area = alloc_vm_area();
                        current->vm_area->vm_start = addr;
                        current->vm_area->vm_end = addr+length-1;
                        current->vm_area->vm_next = next_node;
                        current->vm_area->access_flags = prot;
                        add_to_return = addr;
                    }
                }
            }
            else if (position_to_insert == -EINVAL)
            {
                add_to_return = -EINVAL;

            }
            else
            {//not to be inserted at the head inserted at the previous node
                struct vm_area* node_after_which_inserted = get_node_at_pos(current->vm_area,position_to_insert-1);
                if(node_after_which_inserted == NULL)
                {
                    add_to_return = -EINVAL;
                }
                else
                {
                    struct vm_area* next_node = node_after_which_inserted->vm_next;
                    int next_merge = can_new_be_merged_with_next(next_node,addr,length,prot);
                    int prev_merge = can_new_be_merged_with_prev(node_after_which_inserted,addr,length,prot);
                    long return_add = addr;
                    if(prev_merge && next_merge)
                    {
                        //number will go down only
                        struct vm_area* next_next_node = next_node->vm_next;
                        node_after_which_inserted->vm_end = next_node->vm_end;
                        node_after_which_inserted->vm_next = next_next_node;
                        dealloc_vm_area(next_node);
                        add_to_return = return_add;
                    }
                    else if(prev_merge)
                    {
                        node_after_which_inserted->vm_end += length;
                        add_to_return = return_add;
                    }
                    else if(next_merge)
                    {
                        next_node->vm_start = addr;
                        add_to_return = return_add;
                    }
                    else//no merge
                    {
                        if(count_num_vm_area(current->vm_area) == 128)
                        {
                            add_to_return = -1;
                        }   
                        else
                        {
                            node_after_which_inserted->vm_next = alloc_vm_area();
                            node_after_which_inserted->vm_next->access_flags = prot;
                            node_after_which_inserted->vm_next->vm_start = return_add;
                            node_after_which_inserted->vm_next->vm_next = next_node;
                            node_after_which_inserted->vm_next->vm_end = return_add+length-1;
                            add_to_return = return_add;
                        }
                    }
                }
            }
        }
        // else if ( availability == -1)
        // {
        //     return -EINVAL;
        // }//Not appropriate
        else if(flags&MAP_FIXED)//The map fixed bit is set, and the address was unavailable
        {
            // return -1;
            add_to_return = -EINVAL;
        }
        else
        {//it overlaps(clashes) with some address and hence we need to find it and search for subsequent address from it
            int position_to_insert = find_new_mapping_after(current->vm_area, length, availability);
            if(position_to_insert == -EINVAL)
                position_to_insert = find_new_mapping(current->vm_area, length);
            if(position_to_insert == 1)//to be inserted at the head
            {
                struct vm_area* next_node = current->vm_area;
                if(can_new_be_merged_with_next(next_node,MMAP_AREA_START,length,prot))
                {
                    //I assume that the length was previously 128 or less only, and it cant increase now as it will be merged
                    next_node -> vm_start = MMAP_AREA_START;
                    add_to_return = MMAP_AREA_START;
                }
                else
                {
                    if(count_num_vm_area(current->vm_area) == 128)
                    {
                        add_to_return = -1;
                    }
                    else
                    {
                        current->vm_area = alloc_vm_area();
                        current->vm_area->vm_start = MMAP_AREA_START;
                        current->vm_area->vm_end = MMAP_AREA_START+length-1;
                        current->vm_area->vm_next = next_node;
                        current->vm_area->access_flags = prot;
                        add_to_return = MMAP_AREA_START;
                    }
                }
            }
            else if (position_to_insert == -EINVAL)
            {
                add_to_return = -EINVAL;
            }
            else
            {//not to be inserted at the head inserted at the previous node
                struct vm_area* node_after_which_inserted = get_node_at_pos(current->vm_area,position_to_insert-1);
                if(node_after_which_inserted == NULL)
                {
                    add_to_return = -EINVAL;
                }
                else
                {
                    struct vm_area* next_node = node_after_which_inserted->vm_next;
                    int next_merge = can_new_be_merged_with_next(next_node,node_after_which_inserted->vm_end+1,length,prot);;
                    int prev_merge = can_new_be_merged_with_prev(node_after_which_inserted,node_after_which_inserted->vm_end+1,length,prot);
                    long return_add = node_after_which_inserted->vm_end+1;
                    if(prev_merge && next_merge)
                    {
                        //number will go down only
                        struct vm_area* next_next_node = next_node->vm_next;
                        node_after_which_inserted->vm_end = next_node->vm_end;
                        node_after_which_inserted->vm_next = next_next_node;
                        dealloc_vm_area(next_node);
                        add_to_return = return_add;
                    }
                    else if(prev_merge)
                    {
                        node_after_which_inserted->vm_end += length;
                        add_to_return = return_add;
                    }
                    else if(next_merge)
                    {
                        next_node->vm_start = node_after_which_inserted->vm_end+1;
                        add_to_return = return_add;
                    }
                    else//no merge
                    {
                        if(count_num_vm_area(current->vm_area) == 128)
                        {
                            add_to_return = -1;
                        }
                        else
                        {
                            node_after_which_inserted->vm_next = alloc_vm_area();
                            node_after_which_inserted->vm_next->access_flags = prot;
                            node_after_which_inserted->vm_next->vm_start = return_add;
                            node_after_which_inserted->vm_next->vm_next = next_node;
                            node_after_which_inserted->vm_next->vm_end = return_add+length-1;
                            add_to_return = return_add;
                        }   
                    }
                }
            } 
        }
    }
    add_inc(current->vm_area);
    //IF MAPPING IS DONE MAP THE PHYSICAL PAGES FROM THE ADDRESS BEING RETUNRED.
    if((add_to_return!=-1)&&((flags&MAP_POPULATE)))//SOMETHING WAS MAPPED AND MAP POPULATE IS THERE
    {
        int i = 0;
        while(i<length)//not inclusive as we are talking about the starting length.
        {   
            map_physical_page((unsigned long)osmap(current->pgd), (add_to_return+(u64)i), prot, 0);//If the address is mapped it will be unmapped.,upn = 0 as the pfn is unaasigned
            i+=PAGE_SIZE;
        }
    }


    return add_to_return;
}



/**
 * munmap system call implemenations
 */

void delete_node(struct exec_context *current, struct vm_area* node_to_be_deleted)
{
    struct vm_area* temp = current->vm_area, *prev;
    // If head node itself holds the key to be deleted
    if (temp != NULL && (temp == node_to_be_deleted)) 
    { 
        current->vm_area = temp->vm_next;   // Changed head 
        dealloc_vm_area(temp);               // free old head 
        return; 
    } 
  
    // Search for the key to be deleted, keep track of the 
    // previous node as we need to change 'prev->next' 
    while (temp != NULL && (temp != node_to_be_deleted)) 
    { 
        prev = temp; 
        temp = temp->vm_next; 
    } 
  
    // If key was not present in linked list 
    if (temp == NULL) return; 
  
    // Unlink the node from linked list 
    prev->vm_next = temp->vm_next; 
  
    dealloc_vm_area(temp);  // Free memory 
}


int vm_area_unmap(struct exec_context *current, u64 addr, int length)
{

    add_dec(current->vm_area);
    //PSEUDOCODE    
/**
 * Pseudocode Begins
 * Idea:
 * To Go over each element of the link list, find if it overlaps with the said range and if it does what is the nature of overlap
 *      a.Partial overlap from the starting
 *          only the starting address needs to be changed, as no mergeing is possible
 *      b.Partial overlap from the middle to the end
 *          only the ending address needs to be changed, no merging becomes possiblel now if wasnt possibke earlier
 *      c.Partial overlap in middle
 *          the node is split into 2 and the start,end and next fields need to be assigned appropriately
 *      d.Complete overlap
 *          the node is deleted
 *          //implement delete node functionality,no merging owuld become possible suddenly now as well.
 */
//-------------------------------------------------------------------------------------------------

    int isValid = -1;
    //code begins
    //1.Get page alingned length
    if(length <= 0)
        return -1;//WHAT TO DO IF LENGTH IS ZERO
    length = get_page_aligned_length(length);
    //-------------------------------------------------------------------------------------------------------------------


    //2.Go over each element:-
    //for checking vm_area limit doing it once with type of overlap 128 is sufficient also it can happen only in case 3
    struct vm_area* curr_node = current->vm_area;
    while((curr_node!=NULL)&&(curr_node->vm_start<=addr+length-1))
    {
        //Perform the staps of the Pseudocode
        int type_of_overlap = check_kind_of_overlap(curr_node,addr,length);
        if(type_of_overlap == 1)
        {

            curr_node->vm_start = (long)addr+(long)length;
        }
        else if(type_of_overlap == 2)
        {

            curr_node->vm_end = (long)addr-1;
        }
        else if(type_of_overlap == 3)
        {
            //Partial overlap in middle handle appropriately

            //split the node into 2 as follows
            if(count_num_vm_area(current->vm_area)==128)
            {
                add_inc(current->vm_area);
                return -EINVAL;
            }
            else
            {
                struct vm_area* next = curr_node->vm_next;
                long end_bef_del = curr_node->vm_end;
                curr_node->vm_end = (long)addr-1;
                curr_node->vm_next = alloc_vm_area();
                if(curr_node->vm_next == NULL)
                {
                    add_inc(current->vm_area);
                    return -EINVAL;
                }
                
                curr_node->vm_next->vm_start = (long)addr+(long)length;
                curr_node->vm_next->vm_end = end_bef_del;
                curr_node->vm_next->access_flags = curr_node->access_flags;
                curr_node->vm_next->vm_next = next;
            }
            

        }
        else if(type_of_overlap == 4)//Complete overlap//handle deletion
        {

            delete_node(current,curr_node);
        }
        //Go to the next node.
        curr_node = curr_node->vm_next;
    }
    
    //UNMAPPING THE PAGES
    int i = 0;
    for(i = 0;i<length;i+=PAGE_SIZE)//not inclusive as we are talking about the starting length.
    {
        if((addr+i>=MMAP_AREA_START && addr+i<MMAP_AREA_END))
        {   
            u64 *pte_entry =get_user_pte(current,addr+1,0);
            if(pte_entry != NULL)//physical page has been mapped
            {
                if(get_pfn_info_refcount(get_pfn_info((*pte_entry & FLAG_MASK) >> PAGE_SHIFT)) == 1)
                {

                    do_unmap_user(current, addr+i);//If the address is mapped it will be unmapped.
                }
                else
                {

                    decrement_pfn_info_refcount(get_pfn_info((*pte_entry & FLAG_MASK) >> PAGE_SHIFT));
                    //decrement as a vm_area no longer references this page
                    // stats->user_reg_pages--;//do we need to do this????
                    *pte_entry = 0;  // Clear the PTE
                    //We dont free the page as mutiple vm_areas are mapped to it
                    u64 Address = addr+i;
                    asm volatile ("invlpg (%0);" 
                                    :: "r"(Address) 
                                    : "memory");   // Flush TLB
                }
            
            
            }
            else//physical page is not mapped yet
            {
                //do nothing;
            }
        }
    }
    //UNMAPPING THE PAGES
    //While unmampping the pages,check the refcount

    add_inc(current->vm_area);
    return 0;
    //return isValid;

}