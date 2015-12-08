/*BSD License

Copyright © belongs to the uploader, all rights reserved.

Redistribution and use in source and binary forms, with or without modification, are permitted provided that the following conditions are met:

Redistributions of source code must retain the above copyright notice, with the name of the uploader, and this list of conditions;

Redistributions in binary form must reproduce the above copyright notice, with the name of the uploader, and this list of conditions in the documentation and/or other materials provided with the distribution;
Neither the name of the uploader nor the names of its contributors may be used to endorse or promote products derived from this software without specific prior written permission.
*/

#include <stdio.h>
#include <list>
#include <map>
#include <vector>
#include <deque>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
//#include <hash_map.h>
#include <sys/time.h>

char cwd[1024];

#include "StopWordProcessor.cpp"
#include "StemmingProcessor.cpp"
#include "ParsingModule.h"
#include "Parser.cpp"
#include "load_parser_kernel.cpp"
#include "parser_kernel.cpp"

#include "defs.h"

#define FALSE 0
#define TRUE 1

using namespace std;

#define TEST_DOC_PATH "resources/test-docs"
char base_path[1024];
list<char*> docs;
//list<char *> tokens_list;
int token_total_count = 0;

extern void load_parser_kernel(char* token_array, int tokens_count, int *doc_token_count, int docs_count);

struct timeval load_start;
struct timeval load_end;
struct timeval createvector_start;
struct timeval createvector_end;
struct timeval dump_start;
struct timeval dump_end;

void loadTestDocs() {
    DIR* dir;
    struct dirent* entry;
    struct stat dir_stat;
    char *fileName = NULL;
    char *oldDir;
    int len;
    
    strcat(cwd, "/");
    dir = opendir(cwd);
    if (!dir) {
        cout << "Cannot read directory "<< cwd <<endl;
        return;
    }

    while ((entry = readdir(dir))) {

        /* skip the "." and ".." entries, to avoid loops. */
        if (strcmp(entry->d_name, ".") == 0)
            continue;
        if (strcmp(entry->d_name, "..") == 0)
            continue;

               /* check if the given entry is a directory. */
               if (stat(entry->d_name, &dir_stat) == -1) {
                    perror("stat:");
                    continue;
                }

        if (S_ISDIR(dir_stat.st_mode)) {
            /* Change into the new directory */
            oldDir = (char*)malloc(strlen(cwd)+1);
            strcpy(oldDir, cwd);
            strcat(cwd, entry->d_name);
            if (chdir(entry->d_name) == -1) {
                cout<< "Cannot chdir into "<<entry->d_name<<endl;
                continue;
            }
            /* check this directory */
            loadTestDocs();

            memset(cwd, '\0', 1024);
            strcpy(cwd, oldDir);
            if (chdir("..") == -1) {
                cout << "Cannot chdir back to "<<cwd<<endl;
                exit(1);
            }           
        }
        else
        {
            /*Not a directory. check if the file ends with .txt*/
            len = strlen(entry->d_name);
            if((entry->d_name[len-4] == '.')
               && (entry->d_name[len-3] == 't')
               && (entry->d_name[len-2] == 'x')
               && (entry->d_name[len-1] == 't'))
            {
               fileName = (char*)malloc(strlen(cwd) + strlen(entry->d_name)+1);
               strcpy(fileName, cwd);
               strcat(fileName, entry->d_name);
               docs.push_back(fileName);
            }
        }
    }
}

long calcDiffTime(struct timeval* strtTime, struct timeval* endTime);

int parseDocs(list<char*> docs, int g, int b) 
{
    //HASH_MAP_PARSED_DOCS parsedDocs;
	StopwordProcessor stopwordProcessor;
    StemmingProcessor stemmingProcessor;
	int prev_token_count = 0, i;
    
    Parser parser(stopwordProcessor, stemmingProcessor);

	int *doc_token_count = (int *)malloc(docs.size() * sizeof(int));
        
    list<char*>::iterator it1;
    token_total_count = 0;
    for (i=0, it1=docs.begin() ; it1 != docs.end(); it1++ , i++)
    {
        char *file = *it1;
        
        FILE *fp = fopen(file, "r");
        parser.parseFile(fp);
        fclose(fp);
        
        doc_token_count[i] = token_total_count - prev_token_count;
		dbg printf("Doc %2d : %3d\n",i, doc_token_count[i]);
		prev_token_count = token_total_count; 
    }
	
	dbg printf("Total tokens = %d\n",token_total_count);

	char *token_array;
    printf("start preparing tokens.\n");
    token_array = (char *)malloc(token_total_count *sizeof(char) * TOKEN_MAX_SIZE_PLUS_END);
    printf("malloc is done.\n");
    int j;

    struct timeval prep_start;
    struct timeval prep_end;

    gettimeofday(&prep_start, NULL); 
    list<char*>::iterator it2;
    int token_count = 0;
    for (i=0, it2=docs.begin() ; it2 != docs.end(); it2++ , i++)
    {
        char *file = *it2;
        
        FILE *fp = fopen(file, "r");
        parser.copyFileToken(fp, &token_array[ token_count * TOKEN_MAX_SIZE_PLUS_END ]);
        fclose(fp);
        
        token_count += doc_token_count[i];
    }
	

    /*	for(int i=0; i<tokens_list.size(); i++)
	{
      strcpy(&token_array[i * TOKEN_MAX_SIZE_PLUS_END], *it);
      it++;
      }*/
    gettimeofday(&prep_end, NULL); 
    long prep_time = calcDiffTime(&prep_start, &prep_end);

    printf("preparing time: %ld\n", prep_time);

	load_parser_kernel(token_array, token_total_count, doc_token_count, docs.size(), g, b);

    free(token_array);
}

int main(int argc, char *argv[]/*, char** token_array, int *tokens_count*/) {

    if (argc != 4)
    {
        cout << "usage ./Main base_path_with_trailing_slash number_of_blocks_per_grid number_of_threads_per_block"<<endl;
        exit(1);
    }

    memset(cwd, '\0', 1024);
    memset(base_path, '\0', 1024);
    strcpy(cwd, argv[1]);
    strcpy(base_path, argv[1]);
    strcat(cwd, TEST_DOC_PATH);
    int g = atoi(argv[2]);
    int b = atoi(argv[3]);
    
    if (chdir(cwd) == -1) {
        cout<< "Cannot chdir to "<<cwd<<endl;
        exit(1);
    }           


    gettimeofday(&load_start, NULL); 
    loadTestDocs(); 
    gettimeofday(&load_end, NULL);
    long loadtime = calcDiffTime(&load_start, &load_end);
    printf("loadtime = %ld\n", loadtime);
    gettimeofday(&createvector_start, NULL); 
    //HASH_MAP_VECTOR vectors = createVectors(docs);
	parseDocs(docs, g, b);
    gettimeofday(&createvector_end, NULL); 
    long cvtime = calcDiffTime(&createvector_start, &createvector_end);
    printf("cvtime = %ld\n", cvtime);
    return 0;
}
