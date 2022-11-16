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
};
MallocMetadata* head_malloc = nullptr;
MallocMetadata* tail_malloc = nullptr;
void* smalloc(size_t size){
    if(size == 0 || size > 100000000)
        return nullptr;
    MallocMetadata* temp = head_malloc;
    while (temp != nullptr){
        if(temp->size >= size && temp->is_free){
            temp->is_free = false;
            return temp + 1;
        }
        temp = temp->next;
    }
    void* adr = sbrk(size + sizeof(MallocMetadata));
    if(adr == (void*)(-1))
        return nullptr;
    MallocMetadata* current_meta = (MallocMetadata*)adr;
    current_meta -> next = nullptr;
    current_meta -> is_free = false;
    current_meta -> size = size;
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
    if(size == 0 || num == 0 || size * num > 100000000)
        return nullptr;
    void* ret = smalloc(num * size);
    if(ret == nullptr)
        return nullptr;
    return memset(ret, 0 , num * size);
}
void sfree(void* p){
    if(p == nullptr)
        return;
    MallocMetadata* temp = (MallocMetadata*)p - 1;
    temp->is_free = true;
}
void* srealloc(void* oldp, size_t size){
    if(size == 0 || size > 100000000)
        return nullptr;
    if(oldp == nullptr){
        return smalloc(size);
    }
    MallocMetadata* current = (MallocMetadata*)oldp -1;
    if(current->size >= size)
        return oldp;
    void* new_malloc = smalloc(size);
    if(new_malloc == nullptr)
        return nullptr;
    memmove(new_malloc,oldp,size);
    sfree(oldp);
    return new_malloc;
}
size_t _num_free_blocks(){
    size_t free_block = 0;
    MallocMetadata* temp = head_malloc;
    while (temp != nullptr){
        if(temp->is_free)
            free_block++;
        temp = temp->next;
    }
    return free_block;
}
size_t _num_free_bytes(){
    size_t free_block_size = 0;
    MallocMetadata* temp = head_malloc;
    while (temp != nullptr){
        if(temp->is_free)
            free_block_size += temp->size;
        temp = temp->next;
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
    return num;
}
size_t _num_allocated_bytes(){
    size_t num = 0;
    MallocMetadata* temp = head_malloc;
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
