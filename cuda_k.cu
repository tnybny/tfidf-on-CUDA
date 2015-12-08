/*BSD License

  Copyright © belongs to the uploader, all rights reserved.

  Redistribution and use in source and binary forms, with or without modification, are permitted provided that the following conditions are met:

  Redistributions of source code must retain the above copyright notice, with the name of the uploader, and this list of conditions;

  Redistributions in binary form must reproduce the above copyright notice, with the name of the uploader, and this list of conditions in the documentation and/or other materials provided with the distribution;
  Neither the name of the uploader nor the names of its contributors may be used to endorse or promote products derived from this software without specific prior written permission.
 */

#include <stdio.h>
#include <math.h>
#include "string_funcs.cu"
//#include "hash_funcs.cpp"
#include "defs.h"
//#include "hash_funcs.h"
//#include "cuda_k.h"
#include "hash_funcs_cuda.h"


char *device_local;
unsigned int *token_length_device;
CalcFreqController *token_division_controller_device;
MyHashMapElement *hash_doc_token_sub_tables_device;
MyHashMapElement *hash_doc_token_tables_device;
MyHashMapElement *occ_hash_table_device;
float *bucket_sqrt_sum_device;
float *doc_similarity_matrix_device;
int *doc_rank_matrix_device;
__device__ bool stripPrefixes ( char *str);
__global__ void StripAffixes(char *dev_res, unsigned int *token_length, CalcFreqController *controller, int docs_count);
__global__ void MakeDocHash2(char *dev_mem, unsigned int *token_length, CalcFreqController *controller,
		MyHashMapElement *hash_doc_token_sub_tables, MyHashMapElement *hash_doc_token_tables, int sub_table_size, int table_size, int maxRows, size_t pitch1, size_t pitch2);
__global__ void MakeDocHash(char *dev_mem, unsigned int *token_length, CalcFreqController *controller,
		MyHashMapElement *hash_doc_token_sub_tables, MyHashMapElement *hash_doc_token_tables, int sub_table_size, int table_size, int maxRows, size_t pitch1, size_t pitch2);
size_t pitch1;
size_t pitch2;
__global__ void AddToOccTable(MyHashMapElement *hash_doc_token_tables, MyHashMapElement *occ_hash_table, int numDocs, size_t pitch2);
float *simbase;
int *rankbase;

#define TRUE 1
#define FALSE 0

#define tablesize PACKET_SIZE

//int strCmp(char *str1, char *str2);

/* This is djb2 hashing algorithm by Dan Bernstien, from comp.lang.c*/
__device__ unsigned long computeHashCuda(char *str)
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
__device__ void initHashTableCuda(MyHashMapElement *hme, int tablerange, int subrange)
{
	MyHashMapElement *bucket = hme;
	for (int i = 0; i != tablerange; i++)
	{
		bucket->countInBuc = 0;
		/*dbg{
		  bucket->freq = 0; // TODO not necessary
		  bucket->key = 0xDEADBEAF;
		  bucket->tokenLength = 0;
		  bucket->subkey = 0;
		  for (int j = 0; j < subrange; j++)
		  {
		  (bucket+j)->countInBuc = 0;
		  (bucket+j)->freq = 0;
		  (bucket+j)->key = 0xDEADBEAF;
		  (bucket+j)->tokenLength = 0;
		  }
		  }*/
		bucket += subrange;
	}
}

__device__ bool insertElementCuda(MyHashMapElement *hme, unsigned long key, int keyshift, int bucketsize, int strlength, int initvalue)
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

__device__ int findElementCuda(MyHashMapElement *hme, unsigned long key, int keyshift, int bucketsize, int strlength)
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

__constant__ char prefixes[][16]= { "kilo", "micro", "milli", "intra", "ultra", "mega", "nano", "pico", "pseudo"};

__constant__ char suffixes2[][2][16] = { { "ational", "ate" },
	{ "tional",  "tion" },
	{ "enci",    "ence" },
	{ "anci",    "ance" },
	{ "izer",    "ize" },
	{ "iser",    "ize" },
	{ "abli",    "able" },
	{ "alli",    "al" },
	{ "entli",   "ent" },
	{ "eli",     "e" },
	{ "ousli",   "ous" },
	{ "ization", "ize" },
	{ "isation", "ize" },
	{ "ation",   "ate" },
	{ "ator",    "ate" },
	{ "alism",   "al" },
	{ "iveness", "ive" },
	{ "fulness", "ful" },
	{ "ousness", "ous" },
	{ "aliti",   "al" },
	{ "iviti",   "ive" },
	{ "biliti",  "ble" }};

__constant__ char suffixes3[][2][16] = { { "icate", "ic" },
	{ "ative", "" },
	{ "alize", "al" },
	{ "alise", "al" },
	{ "iciti", "ic" },
	{ "ical",  "ic" },
	{ "ful",   "" },
	{ "ness",  "" }};

__constant__ char suffixes4[][16] = { "al",
	"ance",
	"ence",
	"er",
	"ic",
	"able", "ible", "ant", "ement", "ment", "ent", "sion", "tion",
	"ou", "ism", "ate", "iti", "ous", "ive", "ize", "ise"};


__device__ bool step1(char *str ) {

	char stem[32];
	bool changed = false;
	if ( str[strLen(str)-1] == 's' ) {
		if ( (hasSuffix( str, "sses", stem ))
				|| (hasSuffix( str, "ies", stem)) ){
			str[strLen(str)-2] = '\0';
			changed = true;
		}
		else {
			if ( ( strLen(str) == 1 )
					&& ( str[strLen(str)-1] == 's' ) ) {
				str[0] = '\0';
				return true;
			}
			if ( str[strLen(str)-2 ] != 's' ) {
				str[strLen(str)-1] = '\0';
				changed = true;
			}
		}
	}

	if ( hasSuffix( str,"eed",stem ) ) {
		if ( measure( stem ) > 0 ) {
			str[strLen(str)-1] = '\0';
			changed = true;
		}
	}
	else {
		if (  (hasSuffix( str,"ed",stem ))
				|| (hasSuffix( str,"ing",stem )) ) {
			if (containsVowel( stem ))  {

				if(stem[0]=='\0')
				{
					str[0]='\0';
					changed = true;
				}
				else
				{
					str[strLen(stem)] = '\0';
					changed = true;
				}
				if ( strLen(str) == 1 )
					return changed;

				if ( ( hasSuffix( str,"at",stem) )
						|| ( hasSuffix( str,"bl",stem ) )
						|| ( hasSuffix( str,"iz",stem) ) ) {
					int len = strLen(str);
					str[len-1] = 'e';
					str[len] = '\0';
					changed = true;

				}
				else {
					int length = strLen(str);
					if ( (str[length-1] == str[length-2])
							&& (str[length-1] != 'l')
							&& (str[length-1] != 's')
							&& (str[length-1] != 'z') ) {
						str[length-1]='\0';
						changed = true;
					}
					else
						if ( measure( str ) == 1 ) {
							if ( cvc(str) )
							{
								str[length-1]='e';
								str[length]='\0';
								changed = true;
							}   
						}
				}
			}
		}
	}

	if ( hasSuffix(str,"y",stem) )
		if ( containsVowel( stem ) ) {
			int len = strLen(str);
			str[len-1]='i';
			str[len]='\0';
			changed = true;
		}
	return changed;
}

__device__ bool step2( char *str ) {

	char stem[32];
	int last = sizeof(suffixes2)/(sizeof(char)*2*16); //strange way of calculating length of array
	bool changed = false;

	for ( int index = 0 ; index < last; index++ ) {
		if ( hasSuffix ( str, suffixes2[index][0], stem ) ) {
			if ( measure ( stem ) > 0 ) {
				int stemlen, suffixlen, j;
				stemlen = strLen(stem);
				suffixlen = strLen(suffixes2[index][1]);
				changed = true;
				for(j=0; j<suffixlen; j++)
					str[stemlen+j] = suffixes2[index][1][j];
				str[stemlen+j] = '\0';
			}
		}
	}
	return changed;
}

__device__ bool step3( char *str ) {

	char stem[32];
	int last = sizeof(suffixes3)/(sizeof(char)*2*16); //strange way of calculating length of array/    
	bool changed= false;
	for ( int index = 0 ; index<last; index++ ) {
		if ( hasSuffix ( str, suffixes3[index][0], stem ))
			if ( measure ( stem ) > 0 ) {
				int stemlen, suffixlen, j;
				stemlen = strLen(stem);
				suffixlen = strLen(suffixes3[index][1]);
				changed = true;
				for( j=0; j<suffixlen; j++)
					str[stemlen+j] = suffixes3[index][1][j];
				str[stemlen+j] = '\0';
			}
	}
	return changed ;  
}

__device__ bool step4( char *str ) {

	char stem[32];
	int last = sizeof(suffixes4)/(sizeof(char)*16); //strange way of calculating length of array
	bool changed = false;
	for ( int index = 0 ; index<last; index++ ) {
		if ( hasSuffix ( str, suffixes4[index], stem ) ) {
			changed = true;
			if ( measure ( stem ) > 1 ) {
				str[strLen(stem)] = '\0';
			}
		}
	}
	return changed;
}

__device__ bool step5( char *str ) {

	bool changed = false;
	if ( str[strLen(str)-1] == 'e' ) {
		if ( measure(str) > 1 ) {
			str[strLen(str)-1] = '\0';
			changed = true;
		}
		else
			if ( measure(str) == 1 ) {
				char stem[32];
				int i;
				for ( i=0; i<strLen(str)-1; i++ )
					stem[i] = str[i];
				stem[i] = '\0';

				if ( !cvc(stem) ){
					str[strLen(str)-1] = '\0';
					changed = true;
				}
			}
	}

	if ( strLen(str) == 1 )
		return true;
	if ( (str[strLen(str)-1] == 'l')
			&& (str[strLen(str)-2] == 'l') && (measure(str) > 1) )
		if ( measure(str) > 1 ) {
			str[strLen(str)-1] = '\0';
			changed = true;
		}

	return changed;
}



__device__ bool stripSuffixes(char *str ) {

	bool changed = false;
	changed = step1( str );
	if ( strLen(str) >= 1 )
		changed |= step2( str );
	if ( strLen(str) >= 1 )
		changed |= step3( str );
	if ( strLen(str) >= 1 )
		changed |= step4( str );
	if ( strLen(str) >= 1 )
		changed |= step5( str );
	return changed;
}

__device__ bool stripPrefixes ( char *str) {

	int  newLen, j;
	bool found = false;

	int last = sizeof(prefixes)/(sizeof(char)*16); //strange way of calculating length of array
	for ( int i=0 ; i<last; i++ ) 
	{
		//Find if str starts with prefix prefixes[i]
		found = prefixFind(str, prefixes[i]);
		if (found)
		{
			newLen = strLen(str) - strLen(prefixes[i]);
			for (j=0 ; j < newLen; j++ )
				str[j] = str[j+strLen(prefixes[i])];
			str[j] = '\0';
		}
	}
	return found;
}

void StripAffixesWrapper(char *host_local, unsigned int *token_length_host, CalcFreqController *token_division_controller_host, int tokens_count, int docs_count, int g, int b)
{

	//cudaMalloc
	cudaMalloc(&device_local, tokens_count * sizeof(char) * TOKEN_MAX_SIZE_PLUS_END);
	cudaMalloc(&token_length_device, tokens_count*sizeof(unsigned int));
	cudaMalloc(&token_division_controller_device, docs_count * sizeof(CalcFreqController));

	//cuda Mempcpy
	cudaMemcpy(device_local, host_local, tokens_count * sizeof(char) * TOKEN_MAX_SIZE_PLUS_END, cudaMemcpyHostToDevice);
	cudaMemcpy(token_length_device, token_length_host, tokens_count*sizeof(unsigned int), cudaMemcpyHostToDevice);
	cudaMemcpy(token_division_controller_device, token_division_controller_host, docs_count * sizeof(CalcFreqController), cudaMemcpyHostToDevice);

	//kernel call
	StripAffixes<<<g, b>>>(device_local, token_length_device, token_division_controller_device, docs_count);

	//cuda Memcpy
	cudaMemcpy(host_local, device_local, tokens_count * sizeof(char) * TOKEN_MAX_SIZE_PLUS_END, cudaMemcpyDeviceToHost);
	cudaMemcpy(token_length_host, token_length_device, tokens_count*sizeof(unsigned int), cudaMemcpyDeviceToHost);

	return;
}

__global__ void StripAffixes(char *dev_res, unsigned int *token_length, CalcFreqController *controller, int docs_count)
{
	int numBags = MAX_THREADS;
	int doc = blockIdx.x;
	int tkn = threadIdx.x;
	if(tkn < MAX_THREADS && doc < docs_count)
	{
		__shared__ char tokens[TOKEN_MAX_SIZE_PLUS_END * MAX_THREADS];
		// adjust the token and token_length array pointer according to controller 
		char *base = &dev_res[controller[doc].doc_token_start * TOKEN_MAX_SIZE_PLUS_END];
		unsigned int *token_length_base = &token_length[controller[doc].doc_token_start];

		int tokens_count = controller[doc].doc_token_count;
		int step_count = tokens_count/numBags;
		int remain = tokens_count - step_count * numBags;
		int index = tkn *  TOKEN_MAX_SIZE_PLUS_END;
		if (tkn < remain )
			step_count += 1;

		int *str;
		int step_size = numBags * TOKEN_MAX_SIZE_PLUS_END;

		int *token; 
		token = (int *)&tokens[TOKEN_MAX_SIZE_PLUS_END * tkn];
		int ratio = sizeof(int)/sizeof(char);
		for(int i=0; i< step_count; i++, index+=step_size)
		{
			int tokenLength = token_length_base[index/TOKEN_MAX_SIZE_PLUS_END]/ratio + 1;
			str = (int *)&base[index];
			// copy to shared memory first
			for (int j = 0; j != tokenLength; j++)
				token[j] = str[j];
			bool changed = ToLowerCase( (char *)token);
			changed |= Clean( (char *)token);
			changed |= stripPrefixes((char *)token);
			changed |= stripSuffixes((char *)token);
			if (changed){
				token_length_base[index/TOKEN_MAX_SIZE_PLUS_END] = strLen((char *)token);
				strCpy(&base[index], (char *)token);
			}
		}
	}
	return;
}

void MakeDocHashWrapper( char *host_local, unsigned int *token_length_host, CalcFreqController *token_division_controller_host, MyHashMapElement **hash_doc_token_sub_tables, MyHashMapElement **hash_doc_token_tables, 
		int sub_table_size, int table_size, int docs_count, int g, int b, int maxRows, int i, int tokens_count)
{
	if(i==0)
	{
		//cudaMalloc	
		cudaMallocPitch(&hash_doc_token_sub_tables_device, &pitch1, sub_table_size * sizeof(MyHashMapElement), MAX_GRID_SIZE);
		cudaMallocPitch(&hash_doc_token_tables_device, &pitch2, table_size * sizeof(MyHashMapElement), docs_count);
	}

	dbg printf("loop %d, pitch2 %d\n", i, pitch2);

	//kernel call
	MakeDocHash<<<g, b>>>(device_local, token_length_device, &(token_division_controller_device[i]), hash_doc_token_sub_tables_device, (MyHashMapElement *)((char *)hash_doc_token_tables_device + i*pitch2), sub_table_size, table_size, maxRows, pitch1, pitch2);
	MakeDocHash2<<<g, b>>>(device_local, token_length_device, &(token_division_controller_device[i]), hash_doc_token_sub_tables_device, (MyHashMapElement *)((char *)hash_doc_token_tables_device + i*pitch2), sub_table_size, table_size, maxRows, pitch1, pitch2);

	if(maxRows != 16)
	{
		//cuda Memcpy
		cudaMemcpy(host_local, device_local, tokens_count * sizeof(char) * TOKEN_MAX_SIZE_PLUS_END, cudaMemcpyDeviceToHost);
		cudaMemcpy(token_length_host, token_length_device, tokens_count * sizeof(unsigned int), cudaMemcpyDeviceToHost);		
		
	}
}

__global__ void MakeDocHash(char *dev_mem, unsigned int *token_length, CalcFreqController *controller, 
		MyHashMapElement *hash_doc_token_sub_tables, MyHashMapElement *hash_doc_token_tables, int sub_table_size, int table_size, int maxRows, size_t pitch1, size_t pitch2)
{
	int maxCols = HASH_DOC_TOKEN_NUM_THREADS;
	int col = threadIdx.x;//z
	int row = blockIdx.x;//x
	if(col < HASH_DOC_TOKEN_NUM_THREADS && row < maxRows)
	{
		char *token_base = &dev_mem[controller[row].doc_token_start * TOKEN_MAX_SIZE_PLUS_END];
		unsigned int *token_length_base = &token_length[controller[row].doc_token_start];
		MyHashMapElement *hash_doc_token_sub_table;
		hash_doc_token_sub_table = (MyHashMapElement *)((char*)hash_doc_token_sub_tables + row * pitch1) + (sub_table_size * col / HASH_DOC_TOKEN_NUM_THREADS);
		MyHashMapElement *hash_doc_token_table;
		hash_doc_token_table = (MyHashMapElement *)((char *) hash_doc_token_tables + row * pitch2);

		{// clear the doc hash sub table in each thread
			initHashTableCuda(hash_doc_token_sub_table, HASH_DOC_TOKEN_SUB_TABLE_SIZE, HASH_DOC_TOKEN_BUCKET_SUB_SIZE);
			// clear the doc hash table
			int bucketsPerThread = HASH_DOC_TOKEN_TABLE_SIZE / maxCols;//256/64 = 4
			if (col < HASH_DOC_TOKEN_TABLE_SIZE % maxCols)
				bucketsPerThread += 1;

			MyHashMapElement *bucket = (MyHashMapElement *)hash_doc_token_table + col * HASH_DOC_TOKEN_BUCKET_SIZE;
			for (int i = 0; i != bucketsPerThread; i++)
			{
				bucket->countInBuc = 0;
				dbg{
					bucket->key = 0xDEADBEEF;
					bucket->subkey = 0;
					bucket->freq = 0;
					bucket->tokenLength = 0;
					for (int j = 1; j != HASH_DOC_TOKEN_BUCKET_SIZE; j++)
					{
						(bucket+j)->countInBuc = 0;
						(bucket+j)->freq = j;
						(bucket+j)->subkey = 0;
						(bucket+j)->key = 0xDEADBEAF;
						(bucket+j)->tokenLength = 0;
					}
				}
				bucket += maxCols * HASH_DOC_TOKEN_BUCKET_SIZE;
			}
		}

		int tokens_count = controller[row].doc_token_count;
		int step_count = tokens_count/maxCols;
		int remain = tokens_count - step_count * maxCols;
		int index = col *  TOKEN_MAX_SIZE_PLUS_END;
		if (col < remain )
			step_count += 1;

		//    int *str;
		int step_size = maxCols * TOKEN_MAX_SIZE_PLUS_END;

		for(int i=0; i< step_count; i++, index+=step_size)
		{
			unsigned long key  = computeHashCuda(&token_base[index]);
			insertElementCuda(hash_doc_token_sub_table, key, HASH_DOC_TOKEN_SUB_TABLE_SIZE_LOG2, HASH_DOC_TOKEN_BUCKET_SUB_SIZE, token_length_base[index/TOKEN_MAX_SIZE_PLUS_END], 1);
		}
		//	dbg printf("Done %d,%d\n",row,col);
	}
	return;
}

__global__ void MakeDocHash2(char *dev_mem, unsigned int *token_length, CalcFreqController *controller, 
		MyHashMapElement *hash_doc_token_sub_tables, MyHashMapElement *hash_doc_token_tables, int sub_table_size, int table_size, int maxRows, size_t pitch1, size_t pitch2)
{
	int col = threadIdx.x;//z
	int row = blockIdx.x;//x
	if(col < HASH_DOC_TOKEN_NUM_THREADS && row < maxRows)
	{
		MyHashMapElement *hash_doc_token_sub_table;
		hash_doc_token_sub_table = (MyHashMapElement *)((char*) hash_doc_token_sub_tables + row * pitch1);
		__shared__ MyHashMapElement *hash_doc_token_table;
		hash_doc_token_table = (MyHashMapElement *)((char*) hash_doc_token_tables + row * pitch2);
		hash_doc_token_sub_table += (sub_table_size * col / HASH_DOC_TOKEN_NUM_THREADS);

		// merge sub tables into one doc hash table
		hash_doc_token_sub_table = (MyHashMapElement *)((char*) hash_doc_token_sub_tables + row * pitch1);
		hash_doc_token_sub_table += (col * HASH_DOC_TOKEN_BUCKET_SUB_SIZE);
		for (int i = 0; i != HASH_DOC_TOKEN_NUM_THREADS; i++)
		{
			MyHashMapElement *bucket = hash_doc_token_sub_table;
			int numInBucket = bucket->countInBuc;
			while(numInBucket--)
			{
				unsigned long key = bucket->key;
				insertElementCuda(hash_doc_token_table, key, HASH_DOC_TOKEN_TABLE_SIZE_LOG2, HASH_DOC_TOKEN_BUCKET_SIZE, bucket->tokenLength, bucket->freq);
				bucket++;
			}
			hash_doc_token_sub_table += HASH_DOC_TOKEN_SUB_TABLE_SIZE * HASH_DOC_TOKEN_BUCKET_SUB_SIZE;
		}
	}
	return;
}

__global__ void InitOccTable(MyHashMapElement *occ_hash_table)
{
	int maxCols = 32;
	int col = threadIdx.x;//z
	int row = blockIdx.x;//x
	if(col < maxCols && row < HASH_DOC_TOKEN_TABLE_SIZE/32)
	{
		MyHashMapElement *bucket = &occ_hash_table[((row * maxCols ) + col) * OCC_HASH_TABLE_BUCKET_SIZE];
		bucket->countInBuc = 0;
		dbg{
			bucket->key = 0xDEADBEEF;
			bucket->freq = 0;
			bucket->tokenLength = 0;
			bucket->subkey = 0;
			for (int j = 1; j < OCC_HASH_TABLE_BUCKET_SIZE; j++)
			{
				bucket[j].countInBuc = 0; 
				bucket[j].key = 0xDEADBEEF;
				bucket[j].freq = 0;
				bucket[j].tokenLength = 0;
				bucket[j].subkey = 0;
			}
		}
	}
}

void AddToOccTableWrapper(MyHashMapElement **hash_doc_token_tables, MyHashMapElement *occ_hash_table, int numDocs, int occ_table_size, int g, int b, int table_size)
{
	//cudaMalloc
	cudaMalloc(&occ_hash_table_device, occ_table_size * sizeof(MyHashMapElement));

	//cudaMemcpy
	cudaMemcpy(occ_hash_table_device, occ_hash_table, occ_table_size * sizeof(MyHashMapElement), cudaMemcpyHostToDevice);

	//kernel call
	InitOccTable<<<g, b>>>(occ_hash_table_device);
	AddToOccTable<<<g, b>>>(hash_doc_token_tables_device, occ_hash_table_device, numDocs, pitch2);

	//cudaMemcpy	
	cudaMemcpy(occ_hash_table, occ_hash_table_device, occ_table_size * sizeof(MyHashMapElement),cudaMemcpyDeviceToHost);	
}

__global__ void AddToOccTable(MyHashMapElement *hash_doc_token_tables, MyHashMapElement *occ_hash_table, int numDocs, size_t pitch2)
{
	int maxCols = 32;
	int col = threadIdx.x;//z
	int row = blockIdx.x;//x
	if(col < maxCols && row < HASH_DOC_TOKEN_TABLE_SIZE/32)
	{
		for (int i = 0; i != numDocs; i++)
		{
			MyHashMapElement *hash_doc_token_table = (MyHashMapElement *)((char*)hash_doc_token_tables + i * pitch2);
			MyHashMapElement *bucket = &hash_doc_token_table[(row * maxCols + col) * HASH_DOC_TOKEN_BUCKET_SIZE];
			int numInBucket = bucket->countInBuc;
			while (numInBucket--)
			{
				unsigned long key = bucket->key;
				insertElementCuda(occ_hash_table, key, OCC_HASH_TABLE_SIZE_LOG2, OCC_HASH_TABLE_BUCKET_SIZE, bucket->tokenLength, 1);
				bucket++;
			}
		}
	}
}

__global__ void CalcTfIdf(CalcFreqController *controller,  MyHashMapElement *hash_doc_token_tables, MyHashMapElement *occ_hash_table, int docs_count, float *bucket_sqrt_sum, size_t pitch2)
{
	int maxCols = HASH_DOC_TOKEN_TABLE_SIZE;
	int col = threadIdx.x;//z
	int row = blockIdx.x;//x
	if(row < docs_count && col < maxCols)
	{
		int token_doc_count = controller[row].doc_token_count;
		// 1. calculate the un-normalized tfidf
		MyHashMapElement *bucket = (MyHashMapElement *)((char *)hash_doc_token_tables + row * pitch2);
		bucket += col * HASH_DOC_TOKEN_BUCKET_SIZE;
		int numInBucket = bucket->countInBuc;
		__shared__ float bucketSqrtSum[HASH_DOC_TOKEN_TABLE_SIZE]; 
		bucketSqrtSum[col] = 0.0f;
		while (numInBucket--)
		{
			unsigned long key = bucket->key;
			int occ = findElementCuda(occ_hash_table, key, OCC_HASH_TABLE_SIZE_LOG2, OCC_HASH_TABLE_BUCKET_SIZE, bucket->tokenLength);
			if (occ != 0)  // we should be able to find it in the occ table
			{
				float tf = (float)bucket->freq/token_doc_count;
				float idf = log(float(docs_count)/occ);
				bucket->tfidf = tf * idf;
				bucketSqrtSum[col] += bucket->tfidf * bucket->tfidf;
				dbg {
					bucket->subkey = occ;
				}
			}
			bucket++;
		}
		__syncthreads();
		if(col == 0)
		{
			float sum = 0.0f;
			for(int i = 0; i < maxCols; i++)
				sum += bucketSqrtSum[i];
			bucket_sqrt_sum[row] = sqrt(sum);
		}
	}
}

__global__ void CalcTfIdf2(CalcFreqController *controller,  MyHashMapElement *hash_doc_token_tables, MyHashMapElement *occ_hash_table, int docs_count, float *bucket_sqrt_sum, size_t pitch2)
{
	int maxCols = HASH_DOC_TOKEN_TABLE_SIZE;
	int col = threadIdx.x;//z
	int row = blockIdx.x;//x
	if(row < docs_count && col < maxCols)
	{
		// pthread_barrier_wait();
		// normalize
		float magnitude = bucket_sqrt_sum[row];
		MyHashMapElement *bucket = (MyHashMapElement *)((char *)hash_doc_token_tables + row * pitch2);
		bucket += col * HASH_DOC_TOKEN_BUCKET_SIZE;
		int numInBucket = bucket->countInBuc;
		while (numInBucket--)
		{
			float tfidf = (float)bucket->tfidf;
			tfidf = tfidf / magnitude;
			bucket->tfidf = tfidf;
			bucket++;
		}
	}
}

void CalcTfidfWrapper(CalcFreqController *token_division_controller_host, MyHashMapElement **hash_doc_token_tables_host, MyHashMapElement *occ_hash_table_remote, int docs_count, float *bucket_sqrt_sum, int g, int b, int table_size)
{
	//cudaMalloc
	cudaMalloc(&bucket_sqrt_sum_device, HASH_DOC_TOKEN_TABLE_SIZE * sizeof(float));

	//kernel calls
	CalcTfIdf<<<g, b>>>(token_division_controller_device, hash_doc_token_tables_device, occ_hash_table_device, docs_count, bucket_sqrt_sum_device, pitch2);
	CalcTfIdf2<<<g, b>>>(token_division_controller_device, hash_doc_token_tables_device, occ_hash_table_device, docs_count, bucket_sqrt_sum_device, pitch2);

	//cudaMemcpy
	for(int j=0; j< docs_count;j++)
		cudaMemcpy(hash_doc_token_tables_host[j], (MyHashMapElement *)((char*)hash_doc_token_tables_device + j * pitch2), table_size * sizeof(MyHashMapElement), cudaMemcpyDeviceToHost);

}

__global__ void CalcSimilarities(MyHashMapElement *hash_doc_token_tables, MyHashMapElement *occ_hash_table_remote, float *similarity_matrix, int docs_count, size_t pitch2)
{
	int col = threadIdx.x;
	int row = blockIdx.x;
	int row2 = blockIdx.y;
	int maxCols = HASH_DOC_TOKEN_TABLE_SIZE;
	if(col < HASH_DOC_TOKEN_TABLE_SIZE && row < docs_count && row2 < docs_count)
	{
		MyHashMapElement *hashDoc_token_table1 = (MyHashMapElement *)((char *)hash_doc_token_tables + row * pitch2); 
		MyHashMapElement *hashDoc_token_table2 = (MyHashMapElement *)((char *)hash_doc_token_tables + row2 * pitch2); 
		__shared__ float sim_sum[HASH_DOC_TOKEN_TABLE_SIZE];
		sim_sum[col] = 0.0f;
		MyHashMapElement *bucket1 = hashDoc_token_table1 + col * HASH_DOC_TOKEN_BUCKET_SIZE;

		int num_ele_1 = bucket1->countInBuc;
		while (num_ele_1--)
		{
			MyHashMapElement *bucket2 = hashDoc_token_table2 + col * HASH_DOC_TOKEN_BUCKET_SIZE;
			int num_ele_2 = bucket2->countInBuc;
			int find = 0;
			while (num_ele_2--)
			{
				if ((bucket2->key == bucket1->key) && (bucket2->tokenLength == bucket1->tokenLength))
				{
					find = 1;
					break;
				}
				bucket2++;
			}
			if (find)
				sim_sum[col] += bucket1->tfidf * bucket2->tfidf;

			bucket1++;
		}
		__syncthreads();
		if(col == 0)
		{
			float sum = 0.0f;
			for(int i = 0; i < maxCols; i++)
				sum += sim_sum[i];
			similarity_matrix[docs_count * row + row2] = sum;
		}
	}
}

void CalcSimilaritiesWrapper(MyHashMapElement **hash_doc_token_tables_host, MyHashMapElement *occ_hash_table_remote, float *doc_similarity_matrix_host, int docs_count, int g, int b)
{
	//cudaMalloc
	cudaMalloc(&doc_similarity_matrix_device, docs_count * docs_count * sizeof(float));

	dim3 threadsPerBlock(b, b);
	dim3 numBlocks(g/2,g/2);
	//kernel calls
	CalcSimilarities<<<numBlocks, b>>>(hash_doc_token_tables_device, occ_hash_table_device, doc_similarity_matrix_device, docs_count, pitch2);

	//cudaMemcpy
	cudaMemcpy(doc_similarity_matrix_host, doc_similarity_matrix_device, docs_count * docs_count * sizeof(float),cudaMemcpyDeviceToHost);
}

__global__ void SortSimilarities2(float *similarity_matrix, int *rank_matrix, int docs_count, float *simbase, int *rankbase)
{
	int col = threadIdx.x;
	int row = blockIdx.x;
	if(col < docs_count && row < docs_count)
	{
		simbase = (float *)similarity_matrix+row*docs_count;
		rankbase = (int *)rank_matrix+row * docs_count;
		float my_value = *((float *)simbase+col);
		int myRank = 0;
		for (int i = 0; i != docs_count; i++)
		{
			if (i == col) 
				continue;
			if (*((float *)simbase+i) > my_value)
				myRank = myRank + 1;
		}

		*((int *)rankbase+col) = myRank;
	}
}

void SortSimilarities2Wrapper(float *doc_similarity_matrix_host, int *doc_rank_matrix_host, int docs_count, int g, int b)
{
	//cudaMalloc
	cudaMalloc(&doc_rank_matrix_device, docs_count * docs_count * sizeof(int));
	cudaMalloc(&simbase, docs_count*sizeof(float));
	cudaMalloc(&rankbase, docs_count*sizeof(int));
	
	//kernel call
	SortSimilarities2<<<g, b>>>(doc_similarity_matrix_device, doc_rank_matrix_device, docs_count, simbase, rankbase);

	//cudaMemcpy
	cudaMemcpy(doc_rank_matrix_host, doc_rank_matrix_device, docs_count * docs_count * sizeof(int), cudaMemcpyDeviceToHost);

	//cudaFree
	cudaFree(&doc_rank_matrix_device);
	cudaFree(&doc_similarity_matrix_device);
	cudaFree(&hash_doc_token_tables_device);
	cudaFree(&occ_hash_table_device);
	cudaFree(&token_division_controller_device);
	cudaFree(&bucket_sqrt_sum_device);
	cudaFree(device_local);
	cudaFree(token_length_device);
	cudaFree(hash_doc_token_sub_tables_device);
}


