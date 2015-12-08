#ifndef __PARSING_MODULE__
#define __PARSING_MODULE__

#include <string.h>
#include <iostream>
#include <stdio.h>
#include <sys/types.h>
#include <string>
#include <regex.h>
#include <hash_set.h>
#include <hash_map.h>

using namespace std;

//extern void load_parser_kernel(char** token_array, int tokens_count);

struct eqstr
{
  bool operator()(char *s1, char *s2) const
  {
    return (strcmp(s1,s2) == 0);
  }
};

#define PATH_STOPWORDS  "resources/stopwords/stopwords.tbl"
#define TOKEN_LENGTH_MIN 3;
#define TOKEN_LENGTH_MAX 50;
#define HASH_MAP_TOKENS hash_map<char*, int, hash<char*>, eqstr>
#define TRUE 1
#define FALSE 0

extern char stopwordPath[1024];
extern char base_path[1024];


class StopwordProcessor {
    hash_set<char*, hash<char*>, eqstr> stopWords;

    public:
    StopwordProcessor() 
    {
        char word[50];
        char *stopwrd;
        int ret = 0;
        memset(stopwordPath, '\0', 1024);
        strcpy(stopwordPath, base_path);
        strcat(stopwordPath, PATH_STOPWORDS);
        FILE *fp = fopen(stopwordPath, "r");
        
        while(1)
        {
            memset(word, '\0', 50);
            ret = fscanf(fp, "%s", word);
            if (ret == EOF)
                   break;
            stopwrd = (char*)malloc(strlen(word)+1);
                        strcpy(stopwrd, word);
            stopWords.insert(stopwrd);
            //TODO:lowercase mode
        }   
    }

    int isStopWord(char *word);
};

class StemmingProcessor {
    class NewString {
        public:
            string str;
            NewString() {
                str = "";
            }
    };

    public:
    string Clean( string str );
    string ToLowerCase(string str);
    string stripPrefixes ( string str);
    string stripAffixes ( string str);
    string stripSuffixes( string str ) ;
    int hasSuffix( string word, string suffix, NewString& stem ) ;
    int vowel( char ch, char prev ) ;
    int measure( string stem ) ;
    int containsVowel( string word ) ;
    int cvc( string str ) ;
    string step1( string str );
    string step2( string str );    
    string step3( string str );   
    string step4( string str );    
    string step5( string str );
};

class Parser {
    regex_t  *htmlRegPattern;
    regex_t  *punctRegPattern;
    regex_t  *tokenRegPattern;

    StemmingProcessor stemmingProcessor;
    StopwordProcessor stopwordProcessor;
    int ret;
    
    public:
        HASH_MAP_TOKENS parseFile(FILE *fp);
        HASH_MAP_TOKENS parseString(char *text);
        void parseStream(char *text, HASH_MAP_TOKENS &tokenTable);
        void mapIterate(HASH_MAP_TOKENS tokenTable);
        void copyFileToken(FILE *fp, char *);

        Parser(StopwordProcessor stopProcessor, StemmingProcessor stemProcessor)
        {            
            stopwordProcessor = stopProcessor;
            stemmingProcessor = stemProcessor;            
            
            htmlRegPattern = (regex_t *)malloc(sizeof(regex_t));
            punctRegPattern = (regex_t *)malloc(sizeof(regex_t));
            tokenRegPattern = (regex_t *)malloc(sizeof(regex_t));

            ret = regcomp(htmlRegPattern, "&([^;]{1,10});", REG_EXTENDED);
            ret =regcomp(punctRegPattern,"[]!#$%&'()*+,./:;<=>\"?@^_`{}[~|\\-]", REG_EXTENDED);
            ret =regcomp(tokenRegPattern,"([a-zA-Z]{3,50})", REG_EXTENDED);
        }


    private:
        int readLine(FILE *fp, char **line);
        void parseHtmlPattern(char *text);
        void parsePunctPattern(char *text);
        void parseTokenPattern(char *text, HASH_MAP_TOKENS &tokenTable);
};

#endif
