#ifndef _HASH_FUNCS_CUDA_H_
#define _HASH_FUNCS_CUDA_H_

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <malloc.h>

#include "defs.h"
#define INITIAL_SIZE 300

typedef struct _CalcFreqController 
{
	int doc_index;
	int doc_token_start;
	int doc_token_count;
}CalcFreqController;

typedef struct _MyHashMapElement
{   
	unsigned long key;    
	unsigned int  countInBuc;  // number of element in the sub packet
	unsigned int  freq;
	float tfidf;
	int           docIndex;   // for debug only TODO
	unsigned int  tokenLength; // for debug only TODO
	int subkey;  // for debug only TODO
}MyHashMapElement;
#endif

