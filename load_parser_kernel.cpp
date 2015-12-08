/*BSD License

  Copyright © belongs to the uploader, all rights reserved.

  Redistribution and use in source and binary forms, with or without modification, are permitted provided that the following conditions are met:

  Redistributions of source code must retain the above copyright notice, with the name of the uploader, and this list of conditions;

  Redistributions in binary form must reproduce the above copyright notice, with the name of the uploader, and this list of conditions in the documentation and/or other materials provided with the distribution;
  Neither the name of the uploader nor the names of its contributors may be used to endorse or promote products derived from this software without specific prior written permission.
  */

// includes, system
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <assert.h>
#include <sys/time.h>

// includes, kernels
#include <parser_kernel.cpp>
#include <hash_funcs.h>
#include <defs.h>
#include <list>
#include "cuda_k.h"

int *packet_start_token; // we need as many packets as the number of threads
int *send_tokens_count, *docs_count_arg;
int *packet_doc_map, *doc_size;
float *magnitude_array, *magnitude_res_array;
float *similarity_matrix, *similarity_res_matrix;

extern std::list<char*> docs;
/********************************************************/
//MyHashMapElement **freq_packets_array_remote;
//MyHashMapElement **doc_token_hashtable_remote;  // each doc has its own token hash table
MyHashMapElement *occ_hash_table_remote;
CalcFreqController  *token_division_controller_host;
float *doc_similarity_matrix_host;
int *doc_rank_matrix_host;

struct timeval memcpy_start;
struct timeval memcpy_end;
struct timeval parser_start;
struct timeval parser_end;
struct timeval corpus_start;
struct timeval corpus_end;
struct timeval normalize_start;
struct timeval normalize_end;

void create_remote_hash_tables(MyHashMapElement **hash_doc_token_tables, MyHashMapElement **hash_doc_token_sub_tables, int docs_count, int *sub_table_size, int *table_size, int *occ_table_size);

void free_remote_hash_tables(MyHashMapElement **hash_doc_token_tables, MyHashMapElement **hash_doc_token_sub_tables, int docs_count);
void free_local_buffers();

struct timeval profile_start;
struct timeval profile_end;
struct timeval prep_start;
struct timeval prep_end;

long calcDiffTime(struct timeval* strtTime, struct timeval* endTime)
{
	return(
			endTime->tv_sec*1000000 + endTime->tv_usec
			- strtTime->tv_sec*1000000 - strtTime->tv_usec
	      );

}


void load_parser_kernel(char *token_array, int tokens_count, int *doc_token_count, int docs_count, int g, int b)
{
	// allocate host memory for the string tokens
	char *host_local, *host_res;
	unsigned int *token_length_host;
	int *token_doc_map_local;
	host_local = token_array; //(char *)malloc(32*tokens_count*sizeof(char *));
	token_length_host = (unsigned int *)malloc(tokens_count*sizeof(unsigned int));//holds length of each token
	token_doc_map_local = (int *)malloc(tokens_count * sizeof(int));//mapping from token to document owner
	token_division_controller_host = (CalcFreqController *)malloc(docs_count * sizeof(CalcFreqController));
	doc_similarity_matrix_host = (float *)malloc(docs_count * docs_count * sizeof(float));
	doc_rank_matrix_host = (int *)malloc(docs_count * docs_count * sizeof(int));
	int num_tokens = 0;
	for (int i = 0; i != docs_count; i++)
	{
		token_division_controller_host[i].doc_index = i;
		token_division_controller_host[i].doc_token_start = num_tokens;
		token_division_controller_host[i].doc_token_count = doc_token_count[i];
		num_tokens += doc_token_count[i];
		dbg {
			printf("token_start = %d, token_count = %d\n", token_division_controller_host[i].doc_token_start,
					token_division_controller_host[i].doc_token_count);
		}
	}

	int remain_doc_tokens = doc_token_count[0];
	int cur_doc = 0;
	gettimeofday(&prep_start, NULL); 

	for(int i=0; i<tokens_count; i++)
	{
		int j;
		for(j=0; j<TOKEN_MAX_SIZE_PLUS_END; j++)
		{
			token_array[i * TOKEN_MAX_SIZE_PLUS_END + j] = token_array[i * TOKEN_MAX_SIZE_PLUS_END + j];
			if(token_array[i * TOKEN_MAX_SIZE_PLUS_END + j]=='\0')
				break;
		}

		token_length_host[i] = j;
		token_doc_map_local[i] = cur_doc;
		remain_doc_tokens--;
		if (remain_doc_tokens == 0){
			cur_doc++;
			if (i != tokens_count - 1)
				remain_doc_tokens = doc_token_count[cur_doc];
		}
	}
	assert(remain_doc_tokens == 0);
	assert(cur_doc == docs_count);

	gettimeofday(&prep_end, NULL); 
	long prep_time = calcDiffTime(&prep_start, &prep_end);
	printf("prep token time = %ld\n", prep_time);


	// allocate device memory
	//    char *dev_mem;
	//    dev_mem = (char *) malloc(32*tokens_count*sizeof(char *));
	//    unsigned int *token_length_array_mem;
	//    token_length_array_mem = (unsigned int *) malloc(tokens_count*sizeof(unsigned int));
	int maxCols = MAX_THREADS;
	int maxRows = docs_count; // TODO floor(tokens_count/(threads.x * 2)));
	int x, y, z;

	MyHashMapElement *hash_doc_token_sub_tables_host[MAX_GRID_SIZE];
	MyHashMapElement *hash_doc_token_tables_host[docs_count];
	int sub_table_size, table_size, occ_table_size;
	create_remote_hash_tables(hash_doc_token_tables_host, hash_doc_token_sub_tables_host, docs_count, &sub_table_size, &table_size, &occ_table_size);

	gettimeofday(&profile_start, NULL); 

	gettimeofday(&parser_start, NULL); 

	/* This needs to be parallelized */
	//for (x = 0; x < maxRows; x++)
	//for (z = 0; z < maxCols; z++)
	StripAffixesWrapper(host_local, token_length_host, token_division_controller_host, tokens_count, docs_count, g, b);
	//StripAffixes(host_local, token_length_host, token_division_controller_host, z, x, maxCols); //send_tokens_count);
	dbg {
		for(int i=0; i<tokens_count; i++)
		{
			for(int j=0; j<TOKEN_MAX_SIZE_PLUS_END; j++)
			{
				if(host_local[i*TOKEN_MAX_SIZE_PLUS_END+j]=='\0')
					break;
				//printf("%c",host_local[i*32+j]);
			}
			printf("\n%3d %s %s (%d %d)",i, &token_array[i * TOKEN_MAX_SIZE_PLUS_END], &host_local[i*TOKEN_MAX_SIZE_PLUS_END], token_length_host[i], token_doc_map_local[i]);
		}
	}

	/* This needs to be parallelized */
	//for (x = 0; x < OCC_HASH_TABLE_SIZE/32; x++)
		//for (z = 0; z < 32; z++)
			//InitOccTable(occ_hash_table_remote, z, x, 32);  // TODO make it multi-grid

	maxCols = HASH_DOC_TOKEN_NUM_THREADS;
	//MakeDocHashWrapper(host_local, token_length_host, token_division_controller_host, hash_doc_token_sub_tables_host, hash_doc_token_tables_host, sub_table_size, table_size, docs_count, g, b);
	//dbg printf("Finished stripAffixes, going to count\n");
	for (int i = 0; i != docs_count;) // TODO we can do only one batch
	{ 
		maxRows = min(16, docs_count - i);  // TODO replace the magic number

		//for (x = 0; x < maxRows; x++)
		//for (z = 0; z < maxCols; z++){
		MakeDocHashWrapper(host_local, token_length_host, token_division_controller_host, hash_doc_token_sub_tables_host, hash_doc_token_tables_host, sub_table_size, table_size, docs_count, g, b, maxRows, i, tokens_count);
		//MakeDocHash(host_local, token_length_host, &token_division_controller_host[i], hash_doc_token_sub_tables_host, &hash_doc_token_tables_host[i], sub_table_size, table_size, z, x, maxCols);
		//}
		//for (x = 0; x < maxRows; x++)
		//for (z = 0; z < maxCols; z++){
		//MakeDocHash2Wrapper(host_local, token_length_host, &token_division_controller_host[i], hash_doc_token_sub_tables_host, &hash_doc_token_tables_host[i], sub_table_size, table_size, z, x, maxCols);
		//MakeDocHash2(host_local, token_length_host, &token_division_controller_host[i], hash_doc_token_sub_tables_host, &hash_doc_token_tables_host[i], sub_table_size, table_size, z, x, maxCols);
		//}
		i += maxRows;
	}

	gettimeofday(&parser_end, NULL); 
	long parsetime = calcDiffTime(&parser_start, &parser_end);
	printf("parsetime = %ld\n", parsetime);

	gettimeofday(&corpus_start, NULL); 

	/* This needs to be parallelized */
	//for (x = 0; x < HASH_DOC_TOKEN_TABLE_SIZE/32; x++)
		//for (z = 0; z < 32; z++)
			//{
			//dbg printf("adding to occ table\n");
			AddToOccTableWrapper(hash_doc_token_tables_host, occ_hash_table_remote, docs_count, occ_table_size, g, b, table_size);
			//AddToOccTable(hash_doc_token_tables_host, occ_hash_table_remote, docs_count, z, x, 32);
			//}

	gettimeofday(&corpus_end, NULL); 
	long corpustime = calcDiffTime(&corpus_start, &corpus_end);
	printf("corpustime = %ld\n", corpustime);

	maxCols = HASH_DOC_TOKEN_TABLE_SIZE;
	maxRows = docs_count;

	gettimeofday(&normalize_start, NULL); 

	/* This needs to be parallelized */
	float bucket_sqrt_sum[HASH_DOC_TOKEN_TABLE_SIZE];
	//for (x = 0; x < maxRows; x++) {
		//float sum = 0.0f; /* reduction */
		//for (z = 0; z < maxCols; z++)
			//sum += CalcTfIdf(token_division_controller_host, hash_doc_token_tables_host, occ_hash_table_remote, docs_count, z, x, maxCols, bucket_sqrt_sum);
		//bucket_sqrt_sum[x] = sqrt(sum);
	//}
	CalcTfidfWrapper(token_division_controller_host, hash_doc_token_tables_host, occ_hash_table_remote, docs_count, bucket_sqrt_sum, g, b, table_size);
	//for (x = 0; x < maxRows; x++)
		//for (z = 0; z < maxCols; z++)
			//CalcTfIdf2(token_division_controller_host, hash_doc_token_tables_host, occ_hash_table_remote, docs_count, z, x, maxCols, bucket_sqrt_sum);
	gettimeofday(&normalize_end, NULL); 
	long tfidftime = calcDiffTime(&normalize_start, &normalize_end);
	printf("tfidf = %ld\n", tfidftime);

	/* This needs to be parallelized */
	// each block does a pair similarity
	//for (x = 0; x < maxRows; x++)
		//for (y = 0; y < maxRows; y++) {
			//float sum = 0.0f; /* reduction */
			//for (z = 0; z < maxCols; z++)
				//sum += CalcSimilarities(hash_doc_token_tables_host, occ_hash_table_remote, doc_similarity_matrix_host, docs_count, z, x, y, maxCols);
			//doc_similarity_matrix_host[docs_count * x + y] = sum;
		//}
		CalcSimilaritiesWrapper(hash_doc_token_tables_host, occ_hash_table_remote, doc_similarity_matrix_host, docs_count, g, b);

	/* This needs to be parallelized */
	maxCols = docs_count;
	//for (x = 0; x < maxRows; x++)
		//for (z = 0; z < maxCols; z++)
			//SortSimilarities(doc_similarity_matrix_host, doc_rank_matrix_host, docs_count, z, x, 0);
	//for (x = 0; x < maxRows; x++)
		//for (z = 0; z < maxCols; z++)
			//SortSimilarities2(doc_similarity_matrix_host, doc_rank_matrix_host, docs_count, z, x, 0);
			SortSimilarities2Wrapper(doc_similarity_matrix_host, doc_rank_matrix_host, docs_count, g, b);

	gettimeofday(&profile_end, NULL);
	long profile_time = calcDiffTime(&profile_start, &profile_end);
	printf("total kernel time = %ld\n", profile_time);

	//        CalcIDF
	dbg{
		for (int i = 0 ; i != 16; i++)
			printf("subtable %d address 0x%x\n", i, hash_doc_token_sub_tables_host[i]);

		//       for (int i = 0; i != dimBlock.x; i++)
		//         printf("thread %d's sub table address = 0x%x.\n", i ,token_length_host[i]);

		MyHashMapElement **tables_host = hash_doc_token_tables_host;
		int doc = 39; if (doc < docs_count)//for (int doc = 39; doc != docs_count; doc+)
		{
			printf ("The %d'th docuemnt hash table:\n", doc);
			MyHashMapElement *table = tables_host[doc];
			for (int j = 0; j != HASH_DOC_TOKEN_TABLE_SIZE; j++)
			{
				printf("The %d'th document hash table, the %d'th bucket\n", doc, j);
				for (int ele = 0; ele != HASH_DOC_TOKEN_BUCKET_SIZE; ele++)
				{
					printf("count in bucket(%d),freq(%d), tokenLen(%d),subkey(%d) tfidf(%f) \n", table[ele].countInBuc,
							table[ele].freq, table[ele].tokenLength, table[ele].subkey, table[ele].tfidf);
				}
				table += HASH_DOC_TOKEN_BUCKET_SIZE;
			}
		}
		MyHashMapElement *occ_table_host = occ_hash_table_remote;
		printf("occurence table\n");
		for (int occ = 0; occ != OCC_HASH_TABLE_SIZE; occ++)
		{
			MyHashMapElement *bucket = &occ_table_host[occ * OCC_HASH_TABLE_BUCKET_SIZE];
			printf("occurrence table: the %d'th bucket:\n", occ);
			for (int ele = 0; ele != OCC_HASH_TABLE_BUCKET_SIZE; ele++)
			{
				printf("count in bucket(%d), freq(%d), tokenLen(%d),subkey(%d) \n", bucket[ele].countInBuc,
						bucket[ele].freq, bucket[ele].tokenLength, bucket[ele].subkey);
			}
		}

	}

	dbg {
		printf("similarity matrix: \n");
		for (int doc1 = 0; doc1 != docs_count; doc1++)
		{
			for (int doc2 = 0; doc2 != docs_count; doc2++)
				printf("%5f(%d) ", doc_similarity_matrix_host[doc1*docs_count + doc2], doc_rank_matrix_host[doc1*docs_count + doc2]);
			printf("\n");
		}
	}

	float *sim = doc_similarity_matrix_host;
	int *rank = doc_rank_matrix_host;
	std::list<char*>::const_iterator doc1i = docs.begin();
	for (int doc1 = 0; doc1 != docs_count; doc1++, doc1i++)
	{
		printf("\n%s : \n", &(*doc1i)[strlen(cwd)]);

		for (int r = 0; r != 10; r++)
		{
			int find = 0;
			std::list<char*>::const_iterator doc2i = docs.begin();
			for (int doc2 = 0; doc2 != docs_count; doc2++, doc2i++)
			{
				if (rank[doc1 * docs_count + doc2] == r)
				{
					printf("%5f, %s\n", sim[doc1 * docs_count + doc2], &(*doc2i)[strlen(cwd)]);
					find = 1;
				}
			}
			if (!find) break;
		}
	}


	free_local_buffers();    
}

void create_remote_hash_tables(MyHashMapElement **hash_doc_token_tables, MyHashMapElement **hash_doc_token_sub_tables, int docs_count, int *sub_table_size, int *table_size, int *occ_table_size)
{
	*sub_table_size = HASH_DOC_TOKEN_SUB_TABLE_SIZE*HASH_DOC_TOKEN_NUM_THREADS* HASH_DOC_TOKEN_BUCKET_SUB_SIZE;
	*table_size = HASH_DOC_TOKEN_TABLE_SIZE * HASH_DOC_TOKEN_BUCKET_SIZE;
	*occ_table_size = OCC_HASH_TABLE_SIZE * OCC_HASH_TABLE_BUCKET_SIZE;

	for (int i = 0; i != MAX_GRID_SIZE; i++)
	{
		hash_doc_token_sub_tables[i] = (MyHashMapElement *) malloc(*sub_table_size*sizeof(MyHashMapElement));
	}
	for (int i = 0; i != docs_count; i++)
	{
		hash_doc_token_tables[i] = (MyHashMapElement *) malloc((*table_size)*sizeof(MyHashMapElement));
	}
	occ_hash_table_remote = (MyHashMapElement *) malloc((*occ_table_size) * sizeof(MyHashMapElement));

	//    doc_rank_matrix_remote = (int *) malloc(docs_count * docs_count * sizeof(float));

	printf("Allocating remote memory size = %d K bytes for hash_token_sub_tables\n", (*sub_table_size)*sizeof(MyHashMapElement) * docs_count/1024);
	printf("Allocating remote memory size = %d K bytes for hash_token_tables.\n", (*table_size)*sizeof(MyHashMapElement) * docs_count / 1024);
	printf("Allocating remote memory size = %d K bytes for global occurence table.\n", (*occ_table_size) * sizeof(MyHashMapElement)/1024);
}

void free_local_buffers()
{
	free(doc_similarity_matrix_host);
	/* TODU: free other buffers as well */
}

