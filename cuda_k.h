#include "hash_funcs.h"

void StripAffixesWrapper(char *host_local, unsigned int *token_length_host, CalcFreqController *token_division_controller_host, int tokens_count, int docs_count, int g, int b);
void MakeDocHashWrapper( char *host_local, unsigned int *token_length_host, CalcFreqController *token_division_controller_host, MyHashMapElement **hash_doc_token_sub_tables, MyHashMapElement **hash_doc_token_tables, 
		int sub_table_size, int table_size, int docs_count, int g, int b, int maxRows, int i, int tokens_count);
void AddToOccTableWrapper(MyHashMapElement **hash_doc_token_tables, MyHashMapElement *occ_hash_table, int numDocs, int occ_table_size, int g, int b, int table_size);
void CalcTfidfWrapper(CalcFreqController *token_division_controller_host, MyHashMapElement **hash_doc_token_tables_host, MyHashMapElement *occ_hash_table_remote, int docs_count, float *bucket_sqrt_sum, int g, int b, int table_size);
void CalcSimilaritiesWrapper(MyHashMapElement **hash_doc_token_tables_host, MyHashMapElement *occ_hash_table_remote, float *doc_similarity_matrix_host, int docs_count, int g, int b);
void SortSimilarities2Wrapper(float *doc_similarity_matrix_host, int *doc_rank_matrix_host, int docs_count, int g, int b);
