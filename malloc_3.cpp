#include <stdlib.h>
#include <unistd.h>
#include <cstring>
#include <iostream>
#include <sys/mman.h>

struct MallocMetadata {
    size_t size;
    bool is_free;
    MallocMetadata* next;
    MallocMetadata* prev;
    MallocMetadata* next_list;
    MallocMetadata* prev_list;
};
//list for metadata:
MallocMetadata* head_malloc = nullptr;
MallocMetadata* tail_malloc = nullptr;
//list for free:
MallocMetadata* memory_list_head = nullptr;
MallocMetadata* memory_list_tail = nullptr;
//list for mmap:
MallocMetadata* mmap_list_head = nullptr;
MallocMetadata* mmap_list_tail = nullptr;

void pirnt2();
void pirnt();
/*function for List: */
MallocMetadata* checkIfExistInList(size_t size){
    if(memory_list_head == nullptr)
        return nullptr;
    MallocMetadata* temp = memory_list_head;
    while (temp != nullptr){
        if(temp->size >= size) {
            temp->is_free = false;
            return temp;
        }
        temp = temp->next_list;
    }
    return nullptr;
}
void AddToList(MallocMetadata* add){
    if(memory_list_head == nullptr){
        memory_list_head = add;
        memory_list_tail = add;
        return;
    }
    if(add->size <= memory_list_head->size){
        if(add->size < memory_list_head->size || (size_t)add < (size_t)memory_list_head) {
            memory_list_head->prev_list = add;
            add->next_list = memory_list_head;
            memory_list_head = add;
            return;
        }
    }
    if(add->size >= memory_list_tail->size){
        if(add->size > memory_list_tail->size || (size_t)add > (size_t)memory_list_tail) {
            memory_list_tail->next_list = add;
            add->prev_list = memory_list_tail;
            memory_list_tail = add;
            return;
        }
    }
    MallocMetadata* temp = memory_list_head;
    while (temp != nullptr){
        if(temp->size < add->size) {
            temp = temp->next_list;
            continue;
        }
        if(temp->size == add->size){
            if((size_t)temp > (size_t)add)
                break;
            else
                temp = temp->next_list;
        }
        else {
            break;
        }
    }
    if(temp != nullptr)
        temp->prev_list->next_list = add;
    add->next_list = temp;
    add->prev_list = temp->prev_list;
    temp->prev_list = add;
}
void RemoveFromList(MallocMetadata* remove){
    if(memory_list_head == remove){
        memory_list_head = remove->next_list;
    }
    if(memory_list_tail == remove){
        memory_list_tail = remove->prev_list;
    }
    if(remove->prev_list != nullptr){
        remove->prev_list->next_list = remove->next_list;
    }
    if(remove->next_list != nullptr){
        remove->next_list->prev_list = remove->prev_list;
    }
    remove->prev_list = nullptr;
    remove->next_list = nullptr;
}
/*End Function for List */


size_t Alignment_size(size_t old){
    if(old % 8 == 0)
        return old;
    return old + (8 - (old % 8));
}
void MergeMemmory(MallocMetadata* check){
    MallocMetadata* combine = check->prev;
    if(combine != nullptr && combine->is_free){
	if(check == tail_malloc)
	   tail_malloc = combine;
        RemoveFromList(check);
        RemoveFromList(combine);
        combine->next = check->next;
        if(check->next != nullptr)
            check->next->prev = combine;
        combine->size += check->size + sizeof(MallocMetadata);
        AddToList(combine);
        }
    if(check->next != nullptr && check->next->is_free){
	//printf("XXX \n");
        MergeMemmory(check->next);}
}
void SplitMemmory(MallocMetadata* check, size_t size){
    if(check->size < size + 128 + sizeof(MallocMetadata))
        return;
    MallocMetadata* new_metta = (MallocMetadata*)((size_t)check + sizeof(MallocMetadata) + size);
    new_metta->size = check->size - size - sizeof(MallocMetadata);
    new_metta->next = check->next;
    if(check->next != nullptr){
        check->next->prev = new_metta;
    }
    check->next = new_metta;
    new_metta->prev = check;
    check->is_free = false;
    new_metta->is_free = true;
    new_metta->next_list = nullptr;
    new_metta->prev_list = nullptr;
    check->size = size;
    AddToList(new_metta);
    if(check == tail_malloc)
        tail_malloc = new_metta;
    MergeMemmory(new_metta);
    //pirnt2();
    //pirnt();
}
void* Wilderness(size_t size, bool to_delete){
    size_t to_add = size - tail_malloc->size;
    if(sbrk(to_add) == ((void*)(-1)))
        return nullptr;
    tail_malloc->size = size;
    tail_malloc->is_free = false;
    if(to_delete)
        RemoveFromList(tail_malloc);
    return tail_malloc + 1;
}
void* mmap_alloc(size_t size){
    void* new_alloc = mmap(NULL,size + sizeof(MallocMetadata),PROT_READ | PROT_WRITE,  MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);
    if(new_alloc == MAP_FAILED)
        return nullptr;
    MallocMetadata* new_meta = (MallocMetadata*)new_alloc;
    new_meta->size = size;
    new_meta->is_free = false;
    new_meta->prev_list = nullptr;
    new_meta->next_list = nullptr;
    new_meta->next = nullptr;
    new_meta->prev = nullptr;
   if(mmap_list_head == nullptr){
        mmap_list_head = new_meta;
        mmap_list_tail = new_meta;
    } else{
        new_meta->prev = mmap_list_tail;
        mmap_list_tail->next = new_meta;
        mmap_list_tail = new_meta;
    }
    return new_meta + 1;
}
void* mmap_srealloc(size_t size, MallocMetadata* prev){
  if(size == prev->size)
        return prev + 1;
    if(mmap_list_head == prev)
        mmap_list_head = prev->next;
    if(mmap_list_tail == prev)
        mmap_list_tail = prev->prev;
    if(prev->next != nullptr)
        prev->next->prev = prev->prev;
    if(prev->prev != nullptr)
        prev->prev->next = prev->next;
    void* new_mmap = mmap_alloc(size);
    if(prev->size > size)
        memmove(new_mmap,prev + 1, size);
    else
        memmove(new_mmap,prev + 1, prev->size);
    munmap(prev - 1, prev->size + sizeof(MallocMetadata));
    return new_mmap;
}
/*function: */
void* smalloc(size_t size){
    size_t new_size = Alignment_size(size);
    if(new_size == 0 || new_size > 100000000)
        return nullptr;
    if(new_size >= 128*1024)
        return mmap_alloc(new_size);
    MallocMetadata* addres = checkIfExistInList(new_size);
    if(addres != nullptr){
        RemoveFromList(addres);
        SplitMemmory(addres,new_size);
        return addres + 1;
    }
    if(tail_malloc != nullptr && tail_malloc->is_free){
        return Wilderness(new_size, true);
    }
    if(tail_malloc == nullptr){
	size_t check = (size_t)sbrk(0);
	if(check % 8 != 0)
	   sbrk(8-(check % 8));
   }    
    void* adr = sbrk(new_size + sizeof(MallocMetadata));
    if(adr == (void*)(-1))
        return nullptr;
    MallocMetadata* current_meta = (MallocMetadata*)adr;
    current_meta -> next = nullptr;
    current_meta -> is_free = false;
    current_meta -> size = new_size;
    current_meta->next_list = nullptr;
    current_meta->prev_list = nullptr;
    if(tail_malloc == nullptr){
        current_meta -> prev = nullptr;
        head_malloc = current_meta;
        tail_malloc = current_meta;
    } else{
        tail_malloc -> next = current_meta;
        current_meta->prev = tail_malloc;
        tail_malloc = current_meta;
    }
    return current_meta + 1;
}
void* scalloc(size_t num, size_t size){
    size_t new_size = Alignment_size(size*num);
    if(new_size == 0 || num == 0 || new_size > 100000000)
        return nullptr;
    void* ret = smalloc(new_size);
    if(ret == nullptr)
        return nullptr;
    return memset(ret, 0 , new_size);
}
void sfree(void* p){
    if(p == nullptr)
        return;
    MallocMetadata* temp = (MallocMetadata*)p - 1;
    if(temp->is_free)
        return;
    if(temp->size >= 128*1024){
        if(mmap_list_head == temp)
            mmap_list_head = temp->next;
        if(mmap_list_tail == temp)
            mmap_list_tail = temp->prev;
        if(temp -> next != nullptr)
            temp->next->prev = temp->prev;
        if(temp->prev != nullptr)
            temp->prev->next = temp->next;
        munmap(temp, temp->size + sizeof(MallocMetadata));
        return;
    }
    temp->is_free = true;
    AddToList(temp);
    MergeMemmory(temp);
}
void* srealloc(void* oldp, size_t size){
    size_t new_size = Alignment_size(size);
    if(new_size == 0 || new_size > 100000000)
        return nullptr;
    if(oldp == nullptr){
        return smalloc(new_size);
    }
    MallocMetadata* old = (MallocMetadata*)oldp - 1;
    if(size >= 128 * 1024)
        return mmap_srealloc(new_size,old);
    if(old->size >= size){
	SplitMemmory(old, new_size);
        return oldp;
	}
    if(old->prev != nullptr && old->prev->is_free && old->prev->size + old->size + sizeof(MallocMetadata) >= new_size){
        MallocMetadata* to_return = (MallocMetadata*)old->prev + 1;
        old->prev->size += old->size + sizeof(MallocMetadata);
        old->prev->next = old->next;
        old->prev->is_free = false;
        if(old->next != nullptr)
            old->next->prev = old->prev;
        else
            memory_list_tail = old->prev;
        RemoveFromList(old->prev);
        SplitMemmory(old->prev, new_size);
        memmove(old->prev + 1, old + 1, old->size);
        return to_return;
        }
    if(old->prev != nullptr && old->prev->is_free && tail_malloc == old){
        tail_malloc = old->prev;
        tail_malloc->next = nullptr;
	tail_malloc->size += old->size + sizeof(MallocMetadata);
        memmove(tail_malloc + 1,old + 1, old->size);
        return Wilderness(new_size, true);
    }
    if(tail_malloc == old){
        return Wilderness(new_size, false);
    }
    if(old->next != nullptr && old->next->is_free && old->next->size + old->size + sizeof(MallocMetadata) >= new_size){
        old->size += old->next->size + sizeof(MallocMetadata);
        RemoveFromList(old->next);
        old->next = old->next->next;
        if(old->next != nullptr)
            old->next->prev = old;
        else
            tail_malloc = old;
        SplitMemmory(old,new_size);
        return old + 1;
    }
    if(old->next != nullptr && old->next->is_free && old->prev != nullptr && old->prev->is_free
    && old->next->size + old->prev->size +old->size + 2*sizeof(MallocMetadata) >= new_size){
        MallocMetadata* to_return = (MallocMetadata*)old->prev + 1;
        old->prev->size += old->size + old->next->size + 2*sizeof(MallocMetadata);
        old->prev->next = old->next->next;
        old->prev->is_free = false;
        if(old->next->next != nullptr)
            old->next->next->prev = old->prev;
        else
            tail_malloc = old->prev;
        RemoveFromList(old->prev);
        RemoveFromList(old->next);
        SplitMemmory(old->prev, new_size);
        memmove(to_return,old + 1, old->size);
        return to_return;
    }
    if(old->next != nullptr && old->next->is_free && tail_malloc == old->next &&
        old->prev != nullptr && old->prev->is_free){
        MallocMetadata* to_return = (MallocMetadata*)old->prev + 1;
        RemoveFromList(old->prev);
        RemoveFromList(old->next);
        tail_malloc = old->prev;
        old->prev->next = nullptr;
        old->prev->is_free = false;
        if(old->prev->size + old->size + old->next->size + 2 * sizeof(MallocMetadata) >= new_size){
            old->prev->size = old->prev->size + old->size + old->next->size + 2 * sizeof(MallocMetadata);
            SplitMemmory(old->prev, new_size);
	    memmove(to_return,old + 1, old->size);
            return to_return;
        }
	old->prev->size = old->prev->size + old->size + old->next->size + 2 * sizeof(MallocMetadata);
	memmove(to_return,old + 1, old->size);
        return Wilderness(new_size, false);
    }
    if(old->next != nullptr && old->next->is_free && tail_malloc == old->next){
        RemoveFromList(old->next);
        tail_malloc = old;
	old->size += old->next->size + sizeof(MallocMetadata);
	old->next = nullptr;
        if(old->size >= new_size){
            SplitMemmory(old, new_size);
            return old + 1;
        }
        return Wilderness(new_size, false);
    }
    void* new_adress = smalloc(new_size);
    if(new_adress != nullptr){
	//std::cout << new_size << std::endl;
        memmove(new_adress,old + 1, old->size);
        sfree(oldp);
        return new_adress;
    }
    return oldp;
}

size_t _num_free_blocks(){
    size_t free_block = 0;
    MallocMetadata* temp = memory_list_head;
    while (temp != nullptr){
        free_block++;
        temp = temp->next_list;
    }
    return free_block;
}
size_t _num_free_bytes(){
    size_t free_block_size = 0;
    MallocMetadata* temp = memory_list_head;
    while (temp != nullptr){
        free_block_size += temp->size;
        temp = temp->next_list;
    }
    return free_block_size;
}
size_t _num_allocated_blocks(){
    size_t num = 0;
    MallocMetadata* temp = head_malloc;
    while (temp != nullptr){
        num++;
        temp = temp->next;
    }
    temp = mmap_list_head;
    while (temp != nullptr){
        num++;
        temp = temp->next;
    }
    return num;
}
size_t _num_allocated_bytes(){
    size_t num = 0;
    MallocMetadata* temp = head_malloc;
    while (temp != nullptr){
        num+=temp->size;
        temp = temp->next;
    }
    temp = mmap_list_head;
    while (temp != nullptr){
        num+=temp->size;
        temp = temp->next;
    }
    return num;
}
size_t _num_meta_data_bytes(){
    return _num_allocated_blocks() * sizeof(MallocMetadata);
}
size_t _size_meta_data(){
    return sizeof(MallocMetadata);
}
void pirnt2(){
	std::cout << "list:" << std::endl;
    MallocMetadata* temp = head_malloc;
    while (temp != nullptr){
        std::cout << "adress:" << (size_t)temp << std::endl;
        std::cout << "size:" << temp->size << std::endl;
        temp = temp->next;
    }
}
void pirnt(){
    std::cout << "free list:" << std::endl;
    MallocMetadata* temp = memory_list_head;
    while (temp != nullptr){
        std::cout << "adress:" << (size_t)temp << std::endl;
        std::cout << "size:" << temp->size << std::endl;
        temp = temp->next_list;
    }
}