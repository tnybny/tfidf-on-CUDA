#ifndef _STR_FUNCS_H_
#define _STR_FUNCS_H_

#include <stdio.h>
#include <string.h>
#include "defs.h"

#define TRUE 1
#define FALSE 0

__device__ int strCpy(char *str1, char *str2);
__device__ int strCmp(char *str1, char *str2);
__device__ bool prefixFind(char *str1, char *str2);
__device__ int strLen(char *str);
__device__ bool Clean( char *str );
__device__ bool ToLowerCase(char *str);
__device__ bool hasSuffix( char *word, char *suffix, char *stem);
__device__ int cvc( char *str );
__device__ int vowel( char ch, char prev );
__device__ int measure( char *stem );
__device__ int containsVowel( char *word );

__device__ int cvc( char *str ) 
{
    int length=strLen(str);

    if ( length < 3 )
        return FALSE;

    if ( (!vowel(str[length-1],str[length-2]) )
            && (str[length-1] != 'w')
            && (str[length-1] != 'x')
            && (str[length-1] != 'y')
            && (vowel(str[length-2],str[length-3])) ) {

        if (length == 3) {
            if (!vowel(str[0],'?'))
                return TRUE;
            else
                return FALSE;
        }
        else {
            if (!vowel(str[length-3],str[length-4]) )
                return TRUE;
            else
                return FALSE;
        }
    }

    return FALSE;
}

__device__ int vowel( char ch, char prev ) 
{
    switch ( ch ) {
    case 'a': case 'e': case 'i': case 'o': case 'u':
        return TRUE;
    case 'y': {

            switch ( prev ) {
            case 'a': case 'e': case 'i': case 'o': case 'u':
                return FALSE;
            default:
                return TRUE;
            }
        }
    default :
        return FALSE;
    }
}

__device__ int measure( char *stem ) 
{
    int i=0, count = 0;
    int length = strLen(stem);

    while ( i < length ) {
        for ( ; i < length ; i++ ) {
            if ( i > 0 ) {
                if ( vowel(stem[i],stem[i-1]) )
                    break;
            }
            else {
                if ( vowel(stem[i],'a') )
                    break;
            }
        }

        for ( i++ ; i < length ; i++ ) {
            if ( i > 0 ) {
                if ( !vowel(stem[i],stem[i-1]) )
                    break;
            }
            else {
                if ( !vowel(stem[i],'?') )
                    break;
            }
        }
        if ( i < length ) {
            count++;
            i++;
        }
    }

    return(count);
}

__device__ int containsVowel( char *word ) 
{
	int len = strLen(word);
    for (int i=0 ; i < len; i++ )
        if ( i > 0 ) {
            if ( vowel(word[i],word[i-1]) )
                return TRUE;
        }
        else {
            if ( vowel(word[0],'a') )
                return TRUE;
        }

    return FALSE;
}

__device__ bool hasSuffix( char *word, char *suffix, char *stem) 
{

    char tmp[TOKEN_MAX_SIZE];
    int wordlen, suffixlen;
    
    wordlen = strLen(word);
    suffixlen = strLen(suffix);

    if ( wordlen <= suffixlen )
        return false;
    if (suffixlen > 1)
        if ( word[wordlen-2] != suffix[suffixlen-2] )
            return false;

    for ( int i=0; i<wordlen-suffixlen; i++ )
        tmp[i] = stem[i] = word[i];
    
    for ( int i=0; i<=suffixlen && i<16; i++ )
        tmp[wordlen-suffixlen+i] = suffix[i];

    if ( strCmp( word , tmp) == 0 )
    {
		//word[wordlen-suffixlen] = '\0';
		stem[wordlen-suffixlen] = '\0';
        return true;
    }
    else
        return false;
}

__device__ bool prefixFind(char *str1, char *str2)
{
	bool found = true;
	for(int i=0; str1[i]!='\0' && str2[i]!='\0';i++)
	{
		if(str1[i] != str2[i])
		{
			found = false;
			break;
		}
	}
	return found;
}

__device__ int strCpy(char *str1, char *str2)
{
	int i=0;
	while(1)
	{
		if(str2[i] == '\0' || i >= 31)
		{
			str1[i] = '\0';
			return 0;
		}
		str1[i] = str2[i];
		i++;
	}
}

__device__ int strCmp(char *str1, char *str2)
{
	int i=0;
	while(1)
	{
		if(str1[i] != str2[i])
			return -1;
		if(str1[i] == '\0' || i==31)
			return 0;
		i++;
	}
}

__device__ int strLen(char *str)
{
	int i;
	for(i=0; str[i]!='\0' && i<=31 ; i++);
	
	return i;
}

__device__ bool Clean( char *str ) {
    int j=0;
    bool change = false;
    for ( int i=0; (i < TOKEN_MAX_SIZE_PLUS_END && str[i]!='\0') ; i++ ) 
    {
		if(str[i]=='\0')
		{
			str[j] = str[i];
			break;
		}
        if ( (str[i]>='a'&&str[i]<='z') || (str[i]>='A'&&str[i]<='Z') || (str[i]>='0'&&str[i]<='9') )
          {
            str[j++] = str[i];
            change = true;
          }
    }
    return change;
    //str[j] = '\0';
}   

__device__ bool ToLowerCase(char *str) {
  bool changed = false;
    for(int i = 0;  (i < TOKEN_MAX_SIZE_PLUS_END && str[i]!='\0') ; i++) {
        if ((str[i]>='A'&&str[i]<='Z'))
        {
            str[i] = (str[i]+32);
            changed = true;
        }               
    }
    return changed;
}

#endif // #ifndef _STR_FUNCS_H_
