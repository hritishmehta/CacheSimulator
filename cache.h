#include<stdlib.h>
#include<stdio.h>
#include<math.h>
#include<string.h>

struct Block{
    unsigned long tag;
    unsigned long valid; 
    unsigned long age;
    unsigned long address; 
};

struct Set{
    struct Block* block;
    long assoc; 
};

struct Cache{
    struct Set* set;
    long setSize;
};
