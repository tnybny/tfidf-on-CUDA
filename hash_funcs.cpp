#ifndef _HASH_FUNCS_C_
#define _HASH_FUNCS_C_

#include <stdio.h>
#include <string.h>
#include "hash_funcs.h"
#include "defs.h"

#define TRUE 1
#define FALSE 0

#define tablesize PACKET_SIZE

//int strCmp(char *str1, char *str2);

/* This is djb2 hashing algorithm by Dan Bernstien, from comp.lang.c*/
unsigned long computeHash(char *str)
{
#if 1
	unsigned long hash = 5381;
	int c;

	while (c = *str++)
		hash = ((hash << 5) + hash) + c;  // hash * 33 + c 

	return hash;
#else
	unsigned long hash = 0;
	int c;
	int i = 0;

	while (c = *str++)
	{
		hash = hash * i + c;
		i++;
	}
	return hash;
#endif
}

// my stuff 
void initHashTable(MyHashMapElement *hme, int tablerange, int subrange)
{
	MyHashMapElement *bucket = hme;
	for (int i = 0; i != tablerange; i++)
	{
		bucket->countInBuc = 0;
		dbg{
			bucket->freq = 0; // TODO not necessary
			bucket->key = 0xDEADBEAF;
			bucket->tokenLength = 0;
			bucket->subkey = 0;
			for (int j = 0; j < subrange; j++)
			{
				bucket[j].countInBuc = 0;
				bucket[j].freq = 0;
				bucket[j].key = 0xDEADBEAF;
				bucket[j].tokenLength = 0;
			}
		}
		bucket += subrange;
	}
}

bool insertElement(MyHashMapElement *hme, unsigned long key, int keyshift, int bucketsize, int strlength, int initvalue)
{
	unsigned long newkey = key & ( (1 << keyshift) - 1 );  // clear the MSBs
	MyHashMapElement *bucket = &hme[newkey * bucketsize];
	int numEleInBucket = bucket->countInBuc;
	// search if the same element is in the bucket, if in, incr the frequency
	for (int i = 0; i != numEleInBucket; i++)
	{ 
		if (bucket[i].key == key && bucket[i].tokenLength == strlength) 
		{
			bucket[i].freq+=initvalue;
			return true;
		}
	}

	if (numEleInBucket == bucketsize) return false;  // if bucket full, drop the element TODO 

	bucket[0].countInBuc++;
	bucket[numEleInBucket].key = key;
	bucket[numEleInBucket].freq = initvalue;
	bucket[numEleInBucket].tokenLength = strlength;
	dbg{
		bucket[numEleInBucket].subkey = newkey;
		bucket[numEleInBucket].countInBuc = numEleInBucket + 1;
	}
	return true;
	//  bucket[numEleInBucket].docIndex = 
	//  bucket[numEleInBucket].tokenLength = 
}


int findElement(MyHashMapElement *hme, unsigned long key, int keyshift, int bucketsize, int strlength)
{
	unsigned long newkey = key & ( (1 << keyshift) - 1 );  // clear the MSBs
	MyHashMapElement *bucket = &hme[newkey * bucketsize];
	int numEleInBucket = bucket->countInBuc;
	// search if the same element is in the bucket, if in, incr the frequency
	for (int i = 0; i != numEleInBucket; i++)
	{ 
		if (bucket[i].key == key && bucket[i].tokenLength == strlength) 
			return bucket[i].freq;
	}

	return 0; 
}

#endif // #ifndef _HASH_FUNCS_C_
