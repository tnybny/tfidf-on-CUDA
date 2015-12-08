#include <iostream.h>
#include <stdio.h>
#include <sys/types.h>
#include <regex.h>
#include <string.h>
#include <hash_map.h>
#include <list>
#include "ParsingModule.h"
#include "defs.h"

//extern list<char *> tokens_list;
extern int token_total_count;

int Parser::readLine(FILE *fp, char **line)
{
    char buffer[10000] = {0};
    int i = 0;
    int c;
    
    while ((c=fgetc(fp)) != '\n')
    {
        if (c == EOF)
            return 0;
            
        buffer[i] = c;
        i++;
    }

    *line = (char*)malloc(strlen(buffer) + 1);
    strcpy(*line, buffer);
    return 1;
}

void Parser::parseHtmlPattern(char *text)
{
    int i;
    int result;

    char *strToRead= text;
    int strt_pointer = 0;

    while(1)
    {
        regmatch_t pmatch[1];
        result = regexec(htmlRegPattern, strToRead, 1, pmatch, 0);
        
        if((result != 0) || (pmatch[0].rm_so == -1))
            break;

        /* Replace all html tags by spaces*/
        memset(text + strt_pointer + pmatch[0].rm_so,' ', pmatch[0].rm_eo - pmatch[0].rm_so);
        
        strt_pointer += pmatch[0].rm_eo;
        strToRead = text + strt_pointer;
    }
}

void Parser::parsePunctPattern(char *text)
{
    int i;
    int result;

    char *strToRead= text;
    int strt_pointer = 0;

    while(1)
    {
        regmatch_t pmatch[1];
        result = regexec(punctRegPattern, strToRead, 1, pmatch, 0);
        
        if((result != 0) || (pmatch[0].rm_so == -1))
            break;

        /* Replace all html tags by spaces*/
        memset(text + strt_pointer + pmatch[0].rm_so,' ', pmatch[0].rm_eo - pmatch[0].rm_so);
        
        strt_pointer += pmatch[0].rm_eo;
        strToRead = text + strt_pointer;
    }
}

void Parser::parseTokenPattern(char *text, HASH_MAP_TOKENS &tokenTable)
{
    int i;
    int result;
    int isStopWord;

    char *strToRead= text;
    int strt_pointer = 0;

    const int tokenlen = 50;
    char *token = (char*)malloc(tokenlen);
    while(1)
    {
        regmatch_t pmatch[1];
        result = regexec(tokenRegPattern, strToRead, 1, pmatch, 0);
        
        if((result != 0) || (pmatch[0].rm_so == -1))
            break;

        /* Replace all html tags by spaces*/
        int len = pmatch[0].rm_eo - pmatch[0].rm_so;

        memset(token, '\0', tokenlen);
        memcpy(token, text + strt_pointer+ pmatch[0].rm_so, len);
        
        isStopWord = stopwordProcessor.isStopWord(token); 
        if (isStopWord == 1)
        {
            strt_pointer += pmatch[0].rm_eo;
             strToRead = text + strt_pointer;
             continue;
        }
		
        token_total_count++;

        strt_pointer += pmatch[0].rm_eo;
        strToRead = text + strt_pointer;
    }
    free(token);
}

void Parser::parseStream(char *text, HASH_MAP_TOKENS &tokenTable)
{
   parseHtmlPattern(text);
   parsePunctPattern(text);
   parseTokenPattern(text, tokenTable); 
}

void Parser::mapIterate(HASH_MAP_TOKENS tokenTable)
{
    HASH_MAP_TOKENS::const_iterator it;
    for ( it=tokenTable.begin() ; it != tokenTable.end(); it++ )
    {
        cout << (*it).first << " => " << (*it).second << endl;
    }
}

HASH_MAP_TOKENS Parser::parseFile(FILE *fp)
{
    int ret;
    char *line;
    HASH_MAP_TOKENS tokenTable;
    
    while((ret = readLine(fp, &line)) == 1)
        parseStream(line, tokenTable);

    return tokenTable;
}

HASH_MAP_TOKENS Parser::parseString(char *text)
{
    HASH_MAP_TOKENS tokenTable;
    parseStream(text, tokenTable);
    return tokenTable;
}

void Parser::copyFileToken(FILE *fp, char *dst)
{
  int ret;
  char *line;
  while((ret = readLine(fp, &line)) == 1)
    {
      parseHtmlPattern(line);
      parsePunctPattern(line);

      int i;
      int result;
      int isStopWord;
      
      char *strToRead= line;
      int strt_pointer = 0;
      
      while(1)
        {
          regmatch_t pmatch[1];
          result = regexec(tokenRegPattern, strToRead, 1, pmatch, 0);
          
          if((result != 0) || (pmatch[0].rm_so == -1))
            break;
          
          /* Replace all reg tokens by spaces*/
          int len = pmatch[0].rm_eo - pmatch[0].rm_so;
          len = min(len, TOKEN_MAX_SIZE_PLUS_END - 1);
          //          char *token = (char*)malloc(len+1);
          //          memset(token, '\0', len + 1);
          //          memcpy(token, text + strt_pointer+ pmatch[0].rm_so, len);
          memset(dst, '\0', TOKEN_MAX_SIZE_PLUS_END);
          memcpy(dst, line + strt_pointer+ pmatch[0].rm_so, len);
          isStopWord = stopwordProcessor.isStopWord(dst);
          if (isStopWord == 1)
            {
              strt_pointer += pmatch[0].rm_eo;
              strToRead = line + strt_pointer;
              continue;
            }
          
          //          tokens_list.push_back(token);
          dst += TOKEN_MAX_SIZE_PLUS_END;
          strt_pointer += pmatch[0].rm_eo;
          strToRead = line + strt_pointer;
        }
    }
}
