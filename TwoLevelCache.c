#include<stdlib.h>
#include<stdio.h>
#include<string.h>
#include<math.h>
#include"cache.h"
#Guide on how to input the cache size parameter field
#./TwoLevelCache <L1 cache size><L1 associativity><L1 cache policy><L1 block size> <L2 cache size><L2 associativity><L2 cache policy><trace file>

long memRead = 0; 
long memWrite = 0;
long l1CacheHit = 0; 
long l1CacheMiss = 0; 
long l2CacheHit = 0; 
long l2CacheMiss = 0;
char* policy1; 
char* policy2; 

struct Block temp1 = {0,0,0,0};

struct Block temp2 = {0,0,0,0};

long getSetIndex(unsigned long address, long setSize, long bSize){
    int setCount = (int)(log(setSize)/log(2));
    int blockCount = (int)(log(bSize)/log(2));
    long setIndex = (address>>blockCount) & ((1<<setCount)-1);
    return setIndex; 
}

unsigned long getTag(unsigned long address, long setSize, long bSize){
    int setCount = (int)(log(setSize)/log(2));
    int blockCount = (int)(log(bSize)/log(2));
    unsigned long tag = (address>>(setCount+blockCount));
    return tag;
}

struct Cache* createCache(long assoc, long bSize, long setSize){
    struct Cache* c1 = malloc(sizeof(struct Cache)); 
    c1->setSize = setSize;
    
    struct Set* s1 = malloc(sizeof(struct Set)*setSize);
    c1->set = s1;
    
    for(int i = 0; i<setSize; i++){
        struct Block* b1 = malloc(sizeof(struct Block)*(int)assoc);
        s1[i].block = b1;
        s1[i].assoc = assoc; 
        for(int j = 0; j<s1[i].assoc; j++){
            b1[j].tag = 0;
            b1[j].age = 0; 
            b1[j].valid = 0;
        }
    }
    return c1;
}

void defaultBlockValues(struct Block* b1){
    b1->tag = 0; 
    b1->address = 0; 
    b1->valid = 0; 
}

//returns whether its a cacheMiss or not and increments counters and ages accordingly based on replacement policy
int isCacheMiss(struct Cache* c1, struct Cache* c2, unsigned long tag, long setIndex, int level){
    if(level == 1){
        struct Set* s1 = c1->set;
        struct Block* b1 = s1[setIndex].block; 
        for(int i = 0; i<s1[setIndex].assoc; i++){
            if(b1[i].tag == tag){
                l1CacheHit++;
                if(strcmp(policy1, "lru")==0){
                   unsigned long highAge = b1[0].age;
                    for(int j = 1; j<s1[setIndex].assoc; j++){
                        if(highAge < b1[j].age){
                            highAge = b1[j].age;
                        }
                    }
                    b1[i].age = highAge+1;
                }
                return 1;
            }   
        }
    }
    if(level==2){
        struct Set* s1 = c2->set;
        struct Block* b1 = s1[setIndex].block; 
        for(int i = 0; i<s1[setIndex].assoc; i++){
            if(b1[i].tag == tag){
                l2CacheHit++;
                if(strcmp(policy2, "lru")==0){
                    long highAge = b1[0].age;
                    for(int j = 1; j<s1[setIndex].assoc; j++){
                        if(highAge < b1[j].age){
                            highAge = b1[j].age;
                        }
                    }
                    b1[i].age = highAge+1;
                }
                return 1;
            }   
        }
    }
    return 0; 
}

void addL2(struct Cache* c1, struct Cache* c2, unsigned long address, char operation, long bSize){
    unsigned long tag = getTag(address, c2->setSize, bSize);
    long setIndex = getSetIndex(address, c2->setSize, bSize);
    struct Set* s2 = c2->set;
    struct Block* b2 = s2[setIndex].block;
    //temp2 only gets edited in l1 so if temp2 is 0 that means that this is the first time l2 is being called so the counters don't get incremented wrongly 
    if(temp2.valid == 0){
        int hit = isCacheMiss(c1, c2, tag, setIndex, 2);
        //don't forget to increment age for l2 block here 
        if(hit!=1){
            memRead++; 
            l2CacheMiss++;
            return; 
        }
        else{
            for(int i = 0; i<s2[setIndex].assoc; i++){
                if(b2[i].tag == tag){
                    temp1.address = address; 
                    temp1.valid = 1; 
                    defaultBlockValues(&b2[i]);
                    b2[i].age = 0; 
                    return; 
                }
            }
        }
    }
    else{
    //we check if there's space to put the evicted block within l1 within l2
        int blockUsedCount = 0; 
        for(int i = 0; i<s2[setIndex].assoc; i++){
            if(b2[i].valid == 0){
                b2[i].valid = 1; 
                b2[i].tag = tag; 
                b2[i].address = address; 
                //incrementing age properly 
                unsigned long highAge = b2[0].age;
                for(int j = 1; j<s2[setIndex].assoc; j++){
                    if(highAge<b2[j].age){
                        highAge = b2[j].age;
                    }
                }
                b2[i].age = highAge+1;
                defaultBlockValues(&temp2);
                return;
            }
            blockUsedCount++;
        }   
        //need to evict out of l2 as well 
        if(blockUsedCount == s2[setIndex].assoc){
            //implementing replacement policy(in this case whatever block gets evicted is lost forever)
            unsigned long minAge = b2[0].age;
            struct Block* leastUsed = &b2[0];
            for(int i = 1; i<s2[setIndex].assoc; i++){
                if(minAge>b2[i].age){
                    minAge = b2[i].age;
                    leastUsed = &b2[i];
                }
            }
            leastUsed->tag = tag; 
            leastUsed->address = address; 
            leastUsed->valid = 1; 
            
            //incrementing age properly; 
            unsigned long highAge = b2[0].age;
                for(int j = 1; j<s2[setIndex].assoc; j++){
                    if(highAge<b2[j].age){
                        highAge = b2[j].age;
                    }
                }
            leastUsed->age = highAge+1;
            defaultBlockValues(&temp2);
        }
    }
}

void addL1(struct Cache* c1, struct Cache* c2, unsigned long address, char operation, long bSize){
    unsigned long tag = getTag(address, c1->setSize, bSize);
    long setIndex = getSetIndex(address, c1->setSize, bSize);

    //to check for cacheHit 
    int hit = isCacheMiss(c1,c2,tag, setIndex, 1); 
    struct Set* s1 = c1->set; 
    struct Block* b1 = s1[setIndex].block;
    
    if(hit!=1){
        l1CacheMiss++; 
        //first call to check if it's in l2
        addL2(c1, c2,address, operation, bSize);
        //case of l1 and l2 cache miss
        if(temp1.valid == 0){  
            int blockUsedCount = 0; 
            //need to check ne wtag

            //if there's an empty spot in l1 cache then it fills it up 
            for(int i = 0; i<s1[setIndex].assoc; i++){
                if(b1[i].valid == 0){
                    b1[i].tag = tag; 
                    b1[i].valid = 1;
                    b1[i].address = address; 

                    unsigned long highAge = b1[0].age;
                    for(int j = 0; j<s1[setIndex].assoc; j++){
                        if(highAge<b1[j].age){
                            highAge = b1[j].age; 
                        }
                    }
                    b1[i].age = highAge + 1; 
                    return;
                }
                blockUsedCount++; 
            }
            //if all the blocks are used and we have to evict from l1 cache and put one into l2
            if(blockUsedCount == s1[setIndex].assoc){
                unsigned long minAge = b1[0].age;
                struct Block* leastUsed = &b1[0]; 
                //replacement policy
                for(int i = 1; i<s1[setIndex].assoc; i++){
                    if(minAge>b1[i].age){
                        minAge = b1[i].age;
                        leastUsed = &b1[i];
                    }
                }
                //for second call to addL2 we have to keep track of block that was evicted from l1
                temp2.tag = leastUsed->tag;
                temp2.address = leastUsed->address;
                temp2.valid = 1; 
                
                //setting the evicted block's tag to a different tag and incrementing age as such; 
                leastUsed->tag = tag; 
                leastUsed->address = address; 
                leastUsed->valid = 1; 
                //incrementing age based on algorithm 
                unsigned long highAge = b1[0].age;
                for(int i = 0; i<s1[setIndex].assoc; i++){
                    if(highAge<b1[i].age){
                        highAge = b1[i].age;
                    }
                }
                leastUsed->age  = highAge + 1;

                addL2(c1, c2, temp2.address, operation, bSize);
            }
        }
        //case of l2 hit and l1 miss 
        else{
            unsigned long tag2 = getTag(temp1.address, c1->setSize, bSize);
            long setIndex2 = getSetIndex(temp1.address, c1->setSize, bSize);

            struct Block* b2 = s1[setIndex2].block; 
            int blockedUsedCount = 0; 
            //if l1 isn't full 
            for(int i = 0; i<s1[setIndex2].assoc; i++){
                if(b2[i].valid == 0){
                    b2[i].tag = tag2; 
                    b2[i].valid = 1; 
                    int highAge = b2[0].age;
                    for(int j = 0; j<s1[setIndex2].assoc; j++){
                        if(highAge<b2[j].age){
                            highAge = b2[j].age;
                        }
                    }
                    b2[i].age = highAge + 1; 
                    break; 
                }
                blockedUsedCount++;
            }
            
            //if l1 is full 
            if(blockedUsedCount == s1[setIndex2].assoc){
                unsigned long minAge = b2[0].age;
                struct Block* leastUsed = &b2[0]; 
                //replacement policy
                for(int i = 1; i<s1[setIndex2].assoc; i++){
                    if(minAge>b2[i].age){
                        minAge = b2[i].age;
                        leastUsed = &b2[i];
                    }
                }
                
                temp2.address = leastUsed->address;
                temp2.valid = 1; 

                leastUsed->address = address; 
                leastUsed->tag = tag2;
                leastUsed->valid = 1; 
                
                int highAge = b2[0].age;
                for(int j = 0; j<s1[setIndex2].assoc; j++){
                    if(highAge<b2[j].age){
                        highAge = b2[j].age;
                    }
                }

                leastUsed->age = highAge + 1; 

                addL2(c1, c2, temp2.address, operation, bSize);
                defaultBlockValues(&temp1);


            }

        }
    }
    //if l1 hit then increases counters accordingly 
}

void freeCache(struct Cache* c1){
    struct Set* s1 = c1->set;
    for(int i = 0; i<c1->setSize; i++){
        struct Set s2 = s1[i];
        free(s2.block);
    }
    free(s1);
    free(c1);
}

int main(int argc, char* argv[argc+1]){
    long cacheSize1, cacheAssoc1, cacheBSize1, cacheSetSize1;
    long cacheSize2, cacheAssoc2, cacheBSize2, cacheSetSize2;
    char assocString1[1000];
    char assocString2[1000];
    unsigned long address; 
    char operation; 

    cacheSize1 = atol(argv[1]);
    cacheSize2 = atol(argv[5]);

    for(int i = 0; i<sizeof(argv[2]); i++){
        assocString1[i] = argv[2][6+i];
        assocString2[i] = argv[6][6+i];
    }

    cacheAssoc1 = atol(assocString1);
    cacheAssoc2 = atol(assocString2);

    policy1 = argv[3];
    policy2 = argv[7];

    cacheBSize1 = atol(argv[4]);
    cacheBSize2 = atol(argv[4]);

    cacheSetSize1 = cacheSize1/(cacheAssoc1*cacheBSize1);
    cacheSetSize2 = cacheSize2/(cacheAssoc2*cacheBSize2);

    FILE* fp = fopen(argv[8], "r");

    struct Cache* c1 = createCache(cacheAssoc1, cacheBSize1, cacheSetSize1);
    struct Cache* c2 = createCache(cacheAssoc2, cacheBSize2, cacheSetSize2);

    while(fscanf(fp, "%c %lx\n", &operation, &address) != EOF){
        if(operation == 'W'){
            memWrite++;
        }
        addL1(c1, c2, address, operation, cacheBSize1);
    }

    printf("memread:%lu\n", memRead);
    printf("memwrite:%lu\n", memWrite);
    printf("l1cachehit:%lu\n", l1CacheHit);
    printf("l1cachemiss:%lu\n", l1CacheMiss);
    printf("l2cachehit:%lu\n", l2CacheHit);
    printf("l2cachemiss:%lu\n", l2CacheMiss);

    freeCache(c1);
    freeCache(c2);

    return EXIT_SUCCESS;
}
