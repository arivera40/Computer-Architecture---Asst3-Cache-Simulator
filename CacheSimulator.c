#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<math.h>
#include<ctype.h>

typedef struct Block{
	int valid;
	unsigned long long int tag;
	int lfuIndex;
}Block;

typedef struct Set{
	int fifoIndex;
	Block **blocks;
}Set;

typedef struct Cache{
	int cacheSize;
	int blockSize;
	int setNum;
	int blockNum;
	int reads;
	int writes;
	int hits;
	int misses;
	Set **sets;
}Cache;

void simulation(Cache *cache, FILE *traceFile, int policyNum);
char *toBinary(char *address, char *dest);
void createCache(Cache *cache, int cacheSize, int blockSize, int associativity, int n);
void createSet(Set *sets, Cache *cache);
Block *createBlock();
unsigned long long int getTag(int tagBits, char *binaryAddress);
int getIndex(int tagBits, int indexBits, char *binaryAddress);
int findAddress(Cache *cache, unsigned long long int tag, int index, int policyNum);
void insertAddress(Cache *cache, unsigned long long int tag, int index, int miss, int policyNum);
void pfetchSimulation(Cache *cache, FILE *traceFile, int policyNum);
void prefetch(Cache *cache, char *binaryAddress, int blockSize, int tagBits, int indexBits, int policyNum);
void freeCache(Cache *cache);

void createCache(Cache *cache, int cacheSize, int blockSize, int associativity, int n){
	if(associativity == 1 ){
		cache->cacheSize = cacheSize;
		cache->blockSize = blockSize;
		cache->setNum = (int)cacheSize / blockSize;
		cache->blockNum = 1;
		cache->reads = 0;
		cache->writes = 0;
		cache->hits = 0;
		cache->misses = 0;
		cache->sets = (Set**)malloc(sizeof(Set*) * cache->setNum);
		int i;
		int j;
		for(i=0; i < cache->setNum; i++){
			cache->sets[i] = (Set*)malloc(sizeof(Set));
			createSet(cache->sets[i], cache);
			for(j=0; j < cache->blockNum; j++){
				cache->sets[i]->blocks[j] = createBlock();
			}
		}
	}else if(associativity == 2){
		cache->cacheSize = cacheSize;
		cache->blockSize = blockSize;
		cache->setNum = 1;
		cache->blockNum = (int)cacheSize / blockSize;
		cache->reads = 0;
		cache->writes = 0;
		cache->hits = 0;
		cache->misses = 0;
		cache->sets = (Set**)malloc(sizeof(Set*) * cache->setNum);
		int i;
		int j;
		for(i=0; i < cache->setNum; i++){
			cache->sets[i] = (Set*)malloc(sizeof(Set)); 
			createSet(cache->sets[i], cache);
			for(j=0; j < cache->blockNum; j++){
				cache->sets[i]->blocks[j] = createBlock();
			}
		}
	}else{
		cache->cacheSize = cacheSize;
		cache->blockSize = blockSize;
		cache->setNum = cacheSize / (blockSize * n);
		cache->blockNum = n;
		cache->reads = 0;
		cache->writes = 0;
		cache->hits = 0;
		cache->misses = 0;
		cache->sets = (Set**)malloc(sizeof(Set*) * cache->setNum);
		int i;
		int j;
		for(i=0; i < cache->setNum; i++){
			cache->sets[i] = (Set*)malloc(sizeof(Set));
			createSet(cache->sets[i], cache);
			for(j=0; j < cache->blockNum; j++){
				cache->sets[i]->blocks[j] = createBlock();
			}
		}
	}
	return;
}

void createSet(Set *sets, Cache *cache){
	sets->blocks = malloc(sizeof(Block) * cache->blockNum);
	sets->fifoIndex = 0;
}

Block* createBlock(){
	Block *blocks = malloc(sizeof(Block));
	blocks->tag = 0;
	blocks->valid = 0;
	blocks->lfuIndex = 0;
	return blocks;
}

void simulation(Cache *cache, FILE *traceFile, int policyNum){
	int eof = 0;
	char address[100];
	char rule;

	int offSetBits = (int)(log(cache->blockSize) / log(2));
	int indexBits = (int)(log(cache->setNum) / log(2)) ;
	int tagBits = 48 - offSetBits - indexBits;

	while(eof != 1){
		fscanf(traceFile, "%s ", address);	//Not really address (pc) but used to skip it
		if(strcmp(address, "#eof") == 0){
			break;
		}
		fscanf(traceFile, "%c %s\n", &rule, address);

		char *binaryAddress = (char*)malloc(sizeof(char) * 48 + 1);

		binaryAddress = toBinary(address, binaryAddress);	//stays hex
		unsigned long long int tag = getTag(tagBits, binaryAddress);
		int index = getIndex(tagBits, indexBits, binaryAddress);
		int hitOrMiss = findAddress(cache, tag, index, policyNum);
		if(rule == 'R'){
			if(hitOrMiss == 0){	//hit
				cache->hits++;
			}else if(hitOrMiss == 1){
				cache->reads++;		//cold miss
				cache->misses++;
				insertAddress(cache, tag, index, 1, policyNum);
			}else{		
				cache->reads++;				//Full
				cache->misses++;
				insertAddress(cache, tag, index, 2, policyNum);
			}
		}else{
			cache->writes++;
			if(hitOrMiss == 0){
				cache->hits++;
			}else if(hitOrMiss == 1){
				cache->reads++;
				cache->misses++;
				insertAddress(cache, tag, index, 1, policyNum);
			}else{
				cache->reads++;
				cache->misses++;
				insertAddress(cache, tag, index, 2, policyNum);
			}
		}
		free(binaryAddress);
	}
	printf("Memory reads: %d\n", cache->reads);
	printf("Memory writes: %d\n", cache->writes);
	printf("Cache hits: %d\n", cache->hits);
	printf("Cache misses: %d\n", cache->misses);
	return;
}

void pfetchSimulation(Cache *cache, FILE *traceFile, int policyNum){
	int eof = 0;
	char address[100];
	char rule;

	int offSetBits = (int)(log(cache->blockSize) / log(2));
	int indexBits = (int)(log(cache->setNum) / log(2));
	int tagBits = 48 - offSetBits - indexBits;

	while(eof != 1){
		fscanf(traceFile, "%s ", address);	//Not really address (pc) but used to skip it
		if(strcmp(address, "#eof") == 0){
			break;
		}
		fscanf(traceFile, "%c %s\n", &rule, address);
	
		char *binaryAddress = (char*)malloc(sizeof(char) * 48 + 1);

		binaryAddress = toBinary(address, binaryAddress);
		unsigned long long int tag = getTag(tagBits, binaryAddress);
		int index = getIndex(tagBits, indexBits, binaryAddress);
		
		int hitOrMiss = findAddress(cache, tag, index, policyNum);
		if(rule == 'R'){
			if(hitOrMiss == 0){	//hit
				cache->hits++;
			}else if(hitOrMiss == 1){	//cold miss
				cache->reads++;
				cache->misses++;
				insertAddress(cache, tag, index, 1, policyNum);
				prefetch(cache, binaryAddress, cache->blockSize, tagBits, indexBits, policyNum);
			}else{						//Full
				cache->reads++;
				cache->misses++;
				insertAddress(cache, tag, index, 2, policyNum);
				prefetch(cache, binaryAddress, cache->blockSize, tagBits, indexBits, policyNum);

			}
		}else{

			cache->writes++;
			if(hitOrMiss == 0){
				cache->hits++;
			}else if(hitOrMiss == 1){
				cache->reads++;
				cache->misses++;
				insertAddress(cache, tag, index, 1, policyNum);
				prefetch(cache, binaryAddress, cache->blockSize, tagBits, indexBits, policyNum);

			}else{
				cache->reads++;
				cache->misses++;
				insertAddress(cache, tag, index, 2, policyNum);
				prefetch(cache, binaryAddress, cache->blockSize, tagBits, indexBits, policyNum);

			}
		}
		free(binaryAddress);
	}
	printf("Memory reads: %d\n", cache->reads);
	printf("Memory writes: %d\n", cache->writes);
	printf("Cache hits: %d\n", cache->hits);
	printf("Cache misses: %d\n", cache->misses);
	return;
}

char *toBinary(char *address, char *dest){
	int i;
	int j = 0;
	for(i = 2; i < strlen(address); i++){	//Start at i=2 to skip 0x in address
		if(address[i] == '0'){
			dest[j] = '0';
			dest[j+1] = '0';
			dest[j+2] = '0';
			dest[j+3] = '0';
		}else if(address[i] == '1'){
			dest[j] = '0';
			dest[j+1] = '0';
			dest[j+2] = '0';
			dest[j+3] = '1';
		}else if(address[i] == '2'){
			dest[j] = '0';
			dest[j+1] = '0';
			dest[j+2] = '1';
			dest[j+3] = '0';
		}else if(address[i] == '3'){
			dest[j] = '0';
			dest[j+1] = '0';
			dest[j+2] = '1';
			dest[j+3] = '1';
		}else if(address[i] == '4'){
			dest[j] = '0';
			dest[j+1] = '1';
			dest[j+2] = '0';
			dest[j+3] = '0';
		}else if(address[i] == '5'){
			dest[j] = '0';
			dest[j+1] = '1';
			dest[j+2] = '0';
			dest[j+3] = '1';
		}else if(address[i] == '6'){
			dest[j] = '0';
			dest[j+1] = '1';
			dest[j+2] = '1';
			dest[j+3] = '0';
		}else if(address[i] == '7'){
			dest[j] = '0';
			dest[j+1] = '1';
			dest[j+2] = '1';
			dest[j+3] = '1';
		}else if(address[i] == '8'){
			dest[j] = '1';
			dest[j+1] = '0';
			dest[j+2] = '0';
			dest[j+3] = '0';
		}else if(address[i] == '9'){
			dest[j] = '1';
			dest[j+1] = '0';
			dest[j+2] = '0';
			dest[j+3] = '1';
		}else if(address[i] == 'a'){
			dest[j] = '1';
			dest[j+1] = '0';
			dest[j+2] = '1';
			dest[j+3] = '0';
		}else if(address[i] == 'b'){
			dest[j] = '1';
			dest[j+1] = '0';
			dest[j+2] = '1';
			dest[j+3] = '1';
		}else if(address[i] == 'c'){
			dest[j] = '1';
			dest[j+1] = '1';
			dest[j+2] = '0';
			dest[j+3] = '0';
		}else if(address[i] == 'd'){
			dest[j] = '1';
			dest[j+1] = '1';
			dest[j+2] = '0';
			dest[j+3] = '1';
		}else if(address[i] == 'e'){
			dest[j] = '1';
			dest[j+1] = '1';
			dest[j+2] = '1';
			dest[j+3] = '0';
		}else{
			dest[j] = '1';
			dest[j+1] = '1';
			dest[j+2] = '1';
			dest[j+3] = '1';
		}
		j+=4;
	}
	dest[j] = '\0';

	int zeroLength = 48 - strlen(dest);
	char *zeroExtend = malloc(sizeof(char) * 48 + 1);
	for(i=0; i < zeroLength; i++){
		zeroExtend[i] = '0';
	}
	zeroExtend[zeroLength] = '\0';
	strcat(zeroExtend, dest);

	free(dest);
	return zeroExtend;
}

unsigned long long int getTag(int tagBits, char *binaryAddress){
	int i;
	char *binaryTag = (char*)malloc(sizeof(char) * tagBits + 1);
	for(i=0; i < tagBits; i++){
		binaryTag[i] = binaryAddress[i];
	}
	strcat(binaryTag, "\0");

	int multiplier = 0;
	int tag = 0;
	for(i = strlen(binaryTag) - 1; i > -1; i--){
		tag += ((binaryTag[i] - '0') * (1 << multiplier));
		multiplier++;
	}
	free(binaryTag);
	return tag;
}

int getIndex(int tagBits, int indexBits, char *binaryAddress){
	int i;
	int j;
	char *binaryIndex = (char*)malloc(sizeof(char) * indexBits + 1);
	for(i=tagBits, j=0; i < tagBits + indexBits; i++, j++){
		binaryIndex[j] = binaryAddress[i];
	}
	strcat(binaryIndex, "\0");

	int multiplier = 0;
	int index = 0;
	for(i = strlen(binaryIndex) - 1; i > -1; i--){
		index += ((binaryIndex[i] - '0') * (1 << multiplier));
		multiplier++;
	}
	free(binaryIndex);
	return index;
}

void prefetch(Cache *cache, char *binaryAddress, int blockSize, int tagBits, int indexBits, int policyNum){
	int i;	
	while(blockSize != 0){
		for(i = strlen(binaryAddress) - 1; i >= 0; i--){
			if(binaryAddress[i] == '0'){
				binaryAddress[i] = '1';
				break;
			}else{
				binaryAddress[i] = '0';
			}
		}
		blockSize--;
	}
	unsigned long long int tag = getTag(tagBits, binaryAddress);
	int index = getIndex(tagBits, indexBits, binaryAddress);
	
	int hitOrMiss = findAddress(cache, tag, index, policyNum);
	if(hitOrMiss == 1){
		cache->reads++;
		insertAddress(cache, tag, index, 1, policyNum);
	}else if(hitOrMiss == 2){
		cache->reads++;
		insertAddress(cache, tag, index, 2, policyNum);
	}
	return;
}

int findAddress(Cache *cache, unsigned long long int tag, int index, int policyNum){
	int i;
	int unvalidCounter = 0;
	int validCounter = 0;
	for(i=0; i < cache->blockNum; i++){
		if(cache->sets[index]->blocks[i]->valid == 0){
			unvalidCounter++;
			if(unvalidCounter == cache->blockNum){
				return 1;
			}
		}else if(cache->sets[index]->blocks[i]->tag == tag){
			if(policyNum == 2){
				int breakPoint = cache->sets[index]->blocks[i]->lfuIndex;
				cache->sets[index]->blocks[i]->lfuIndex = cache->blockNum;
				int j;
				for(j=0; j < cache->blockNum; j++){
					if(cache->sets[index]->blocks[j]->valid == 0){
						break;
					}
					if(cache->sets[index]->blocks[j]->lfuIndex > breakPoint){
						cache->sets[index]->blocks[j]->lfuIndex--;
					}
				}
			}
			return 0;
		}else{
			validCounter++;
		}
	}
	if(validCounter == cache->blockNum){
		return 2;
	}else{
		return 1;
	}
}

void insertAddress(Cache *cache, unsigned long long int tag, int index, int miss, int policyNum){
	int i;
	if(policyNum == 1){
		if(miss == 1){
			for(i=0; i < cache->blockNum; i++){
				if(cache->sets[index]->blocks[i]->valid == 0){
					cache->sets[index]->blocks[i]->tag = tag;
					cache->sets[index]->blocks[i]->valid = 1;
					break;
				}
			}	
		}else{
			cache->sets[index]->blocks[cache->sets[index]->fifoIndex]->tag = tag;
			cache->sets[index]->fifoIndex++;
			if(cache->sets[index]->fifoIndex == cache->blockNum){
				cache->sets[index]->fifoIndex = 0;
			}
		}
	}else{
		if(miss == 1){
			for(i=0; i < cache->blockNum; i++){
				if(cache->sets[index]->blocks[i]->valid == 0){
					cache->sets[index]->blocks[i]->tag = tag;
					cache->sets[index]->blocks[i]->valid = 1;
					cache->sets[index]->blocks[i]->lfuIndex = cache->blockNum;
					int j;
					for(j=0; j < cache->blockNum; j++){
						if(cache->sets[index]->blocks[j]->tag != tag){
							cache->sets[index]->blocks[j]->lfuIndex--;
						}
					}
				}
			}
		}else{
			for(i=0; i < cache->blockNum; i++){
				if(cache->sets[index]->blocks[i]->lfuIndex == 1){
					cache->sets[index]->blocks[i]->tag = tag;
					int breakPoint = cache->sets[index]->blocks[i]->lfuIndex;
					cache->sets[index]->blocks[i]->lfuIndex = cache->blockNum;
					int j;
					for(j=0; j < cache->blockNum; j++){
						if(cache->sets[index]->blocks[j]->lfuIndex > breakPoint){
							cache->sets[index]->blocks[j]->lfuIndex--;
						}
					}
				}
			}
		}
	}
	return;
}

void freeCache(Cache *cache){
	int i;
	int j;
	for(i=0; i < cache->setNum; i++){
		for(j=0; j < cache->blockNum; j++){
			free(cache->sets[i]->blocks[j]);
		}
		free(cache->sets[i]->blocks);
		free(cache->sets[i]);
	}
	free(cache);
	return;
}

int main(int argc, char** argv){
	int cacheSize = atoi(argv[1]);
	char *associativity = argv[2];
	char *cachePolicy = argv[3];
	int blockSize = atoi(argv[4]);

	if(cacheSize % 2 != 0 && cacheSize == 0){
		printf("error\n");
		return 0;
	}
	if(blockSize % 2 != 0 && blockSize == 0){
		printf("error\n");
		return 0;
	}	
	int policyNum = 0;
	if(strcmp(cachePolicy, "fifo") == 1 && strcmp(cachePolicy, "lru") == 1){
		printf("error\n");
		return 0;
	}else if(strcmp(cachePolicy, "fifo") == 0){
		policyNum = 1;
	}else{
		policyNum = 2;
	}
	
	FILE *traceFile;
	FILE *traceFile2;
	traceFile = fopen(argv[5], "r");
	traceFile2 = fopen(argv[5], "r");

	if(traceFile == NULL){
		printf("error\n");
		return 0;
	}

	if(strcmp(associativity, "direct") == 0){
		Cache *cache1 = (Cache*)malloc(sizeof(Cache));
		createCache(cache1, cacheSize, blockSize, 1, 0);
		printf("no-prefetch\n");
		simulation(cache1, traceFile, policyNum);
		Cache *cache2 = (Cache*)malloc(sizeof(Cache));
		createCache(cache2, cacheSize, blockSize, 1, 0);
		printf("with-prefetch\n");
		pfetchSimulation(cache2, traceFile2, policyNum);
		freeCache(cache1);
		freeCache(cache2);
	}else if(strcmp(associativity, "assoc") == 0){
		Cache *cache1 = (Cache*)malloc(sizeof(Cache));
		createCache(cache1, cacheSize, blockSize, 2, 0);
		printf("no-prefetch\n");
		simulation(cache1, traceFile, policyNum);
		Cache *cache2 = (Cache*)malloc(sizeof(Cache));
		createCache(cache2, cacheSize, blockSize, 2, 0);
		printf("with-prefetch\n");
		pfetchSimulation(cache2, traceFile2, policyNum);
		freeCache(cache1);
		freeCache(cache2);
	}else{
		char temp[10];
		strncpy(temp, associativity, 6);
		temp[6] = '\0';
		int n;
		if(strcmp(temp, "assoc:") == 0){
			int i;
			char nstr[10];
			int index = 0;
			for(i=6; i < strlen(associativity); i++){
				if(isdigit(associativity[i]) == 2048){
					nstr[index] = associativity[i];
					index++;
				}else{
					printf("error\n");
					return 0;
				}
			}
			nstr[index] = '\0';
			n = atoi(nstr);
			if(n % 2 != 0 && n != 0){
				printf("error\n");
				return 0;
			}
		}else{
			printf("error\n");
			return 0;
		}
		Cache *cache1 = (Cache*)malloc(sizeof(Cache));
		createCache(cache1, cacheSize, blockSize, 3, n);
		printf("no-prefetch\n");
		simulation(cache1, traceFile, policyNum);
		Cache *cache2 = (Cache*)malloc(sizeof(Cache));
		createCache(cache2, cacheSize, blockSize, 3, n);
		printf("with-prefetch\n");
		pfetchSimulation(cache2, traceFile2, policyNum);
		freeCache(cache1);
		freeCache(cache2);
	}
	return 0;
}
