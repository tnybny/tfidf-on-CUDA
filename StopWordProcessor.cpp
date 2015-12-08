#include <stdio.h>
#include <hash_set.h>
#include <unistd.h>
#include "ParsingModule.h"

using namespace std;
char stopwordPath[1024];

int StopwordProcessor::isStopWord(char *word) {
    int ret;
    hash_set<char*, hash<char*>, eqstr>::const_iterator it = stopWords.find(word);
    ret = (it != stopWords.end()?1:0);
    return ret;
}   
