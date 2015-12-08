#ifndef _HASH_FUNCS_H_
#define _HASH_FUNCS_H_

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

  //  unsigned int  padding;
  //    char token[TOKEN_MAX_SIZE];
  
  /*    float value;
	int value1;
	unsigned long value2;
	unsigned long value3;
	unsigned long value4;
	float tfidf;
	float tfidf_normalized; */
}MyHashMapElement;

unsigned long computeHash(char *str);

void initHashTable(MyHashMapElement *hme, int tablerange, int subrange);
bool insertElement(MyHashMapElement *hme, unsigned long key, int keyshift, int bucketsize, int strlength, int initvalue);
int findElement(MyHashMapElement *hme, unsigned long key, int keyshift, int bucketsize, int strlength);
#endif
