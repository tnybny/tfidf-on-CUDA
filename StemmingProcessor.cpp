#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include "ParsingModule.h"

using namespace std;
string StemmingProcessor::Clean( string str ) {
    size_t last = str.length();

    string temp("");

    for ( int i=0; i < last; i++ ) {
        if ( isalnum( str[i]) )
            temp += str[i];
    }

    return temp;
}   

string StemmingProcessor::ToLowerCase(string str) {
    int i = 0;
    

    for(i = 0; i < str.length(); i++) {
        if (isupper(str[i]))
        {
            str[i] = tolower(str[i]);
        }               
    }
   return str;
}

string StemmingProcessor::stripPrefixes ( string str) {

    string prefixes[] = { "kilo",
            "micro",
            "milli", "intra", "ultra", "mega", "nano", "pico", "pseudo"};
    
    int found;

    int last = sizeof(prefixes)/(sizeof(int*)); /*strange way of calculating length of array*/
    for ( int i=0 ; i<last; i++ ) 
    {
        /*Find if str starts with prefix prefixes[i]*/
        found = str.find(prefixes[i]);
        if (found == 0)
        {
            string temp("");
            for ( int j=0 ; j < str.length() - prefixes[i].length(); j++ )
                temp += str[j+prefixes[i].length()];
            return temp;
        }
    }

    return str;
}

int StemmingProcessor::hasSuffix( string word, string suffix, NewString &stem ) {

    string tmp = "";

    if ( word.length() <= suffix.length() )
        return FALSE;
    if (suffix.length() > 1)
        if ( word[word.length()-2] != suffix[suffix.length()-2] )
            return FALSE;

    stem.str = "";

    for ( int i=0; i<word.length()-suffix.length(); i++ )
        stem.str += word[i];
    tmp = stem.str;

    for ( int i=0; i<suffix.length(); i++ )
        tmp += suffix[i];

    if ( tmp.compare( word ) == 0 )
        return TRUE;
    else
        return FALSE;
}

int StemmingProcessor::vowel( char ch, char prev ) {
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

int StemmingProcessor::measure( string stem ) {

    int i=0, count = 0;
    int length = stem.length();

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

int StemmingProcessor:: containsVowel( string word ) {

    for (int i=0 ; i < word.length(); i++ )
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

int StemmingProcessor:: cvc( string str ) {
    int length=str.length();

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


string StemmingProcessor::step1( string str ) {

    NewString stem;

    if ( str[str.length()-1] == 's' ) {
        if ( (hasSuffix( str, "sses", stem ))
                || (hasSuffix( str, "ies", stem)) ){
            string tmp = "";
            for (int i=0; i<str.length()-2; i++)
                tmp += str[i];
            str = tmp;
        }
        else {
            if ( ( str.length() == 1 )
                    && ( str[str.length()-1] == 's' ) ) {
                str = "";
                return str;
            }
            if ( str[str.length()-2 ] != 's' ) {
                string tmp = "";
                for (int i=0; i<str.length()-1; i++)
                    tmp += str[i];
                str = tmp;
            }
        }
    }

    if ( hasSuffix( str,"eed",stem ) ) {
        if ( measure( stem.str ) > 0 ) {
            string tmp = "";
            for (int i=0; i<str.length()-1; i++)
                tmp += str[i];
            str = tmp;
        }
    }
    else {
        if (  (hasSuffix( str,"ed",stem ))
                || (hasSuffix( str,"ing",stem )) ) {
            if (containsVowel( stem.str ))  {

                string tmp = "";
                for ( int i = 0; i < stem.str.length(); i++)
                    tmp += str[i];
                str = tmp;
                if ( str.length() == 1 )
                    return str;

                if ( ( hasSuffix( str,"at",stem) )
                        || ( hasSuffix( str,"bl",stem ) )
                        || ( hasSuffix( str,"iz",stem) ) ) {
                    str += "e";

                }
                else {
                    int length = str.length();
                    if ( (str[length-1] == str[length-2])
                            && (str[length-1] != 'l')
                            && (str[length-1] != 's')
                            && (str[length-1] != 'z') ) {

                        tmp = "";
                        for (int i=0; i<str.length()-1; i++)
                            tmp += str[i];
                        str = tmp;
                    }
                    else
                    if ( measure( str ) == 1 ) {
                        if ( cvc(str) )
                            str += "e";
                    }
                }
            }
        }
    }

    if ( hasSuffix(str,"y",stem) )
        if ( containsVowel( stem.str ) ) {
            string tmp = "";
            for (int i=0; i<str.length()-1; i++ )
                tmp += str[i];
            str = tmp + "i";
        }
    return str;
}

string StemmingProcessor::step2( string str ) {

    string suffixes[][2] = { { "ational", "ate" },
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
    NewString stem;
    int last = sizeof(suffixes)/(sizeof(int*)*2); /*strange way of calculating length of array*/


    for ( int index = 0 ; index < last; index++ ) {
        if ( hasSuffix ( str, suffixes[index][0], stem ) ) {
            if ( measure ( stem.str ) > 0 ) {
                str = stem.str + suffixes[index][1];
                return str;
            }
        }
    }

    return str;
}

string StemmingProcessor::step3( string str ) {

    string suffixes[][2] = { { "icate", "ic" },
            { "ative", "" },
            { "alize", "al" },
            { "alise", "al" },
            { "iciti", "ic" },
            { "ical",  "ic" },
            { "ful",   "" },
            { "ness",  "" }};
    NewString stem;
    int last = sizeof(suffixes)/(sizeof(int*)*2); /*strange way of calculating length of array*/    

    for ( int index = 0 ; index<last; index++ ) {
        if ( hasSuffix ( str, suffixes[index][0], stem ))
            if ( measure ( stem.str ) > 0 ) {
                str = stem.str + suffixes[index][1];
                return str;
            }
    }
    return str;
}

string StemmingProcessor::step4( string str ) {

    string suffixes[] = { "al",
            "ance",
            "ence",
            "er",
            "ic",
            "able", "ible", "ant", "ement", "ment", "ent", "sion", "tion",
            "ou", "ism", "ate", "iti", "ous", "ive", "ize", "ise"};

    NewString stem;
    int last = sizeof(suffixes)/(sizeof(int*)); /*strange way of calculating length of array*/    

    for ( int index = 0 ; index<last; index++ ) {
        if ( hasSuffix ( str, suffixes[index], stem ) ) {

            if ( measure ( stem.str ) > 1 ) {
                str = stem.str;
                return str;
            }
        }
    }
    return str;
}

string StemmingProcessor::step5( string str ) {

    if ( str[str.length()-1] == 'e' ) {
        if ( measure(str) > 1 ) {
            string tmp = "";
            for ( int i=0; i<str.length()-1; i++ )
                tmp += str[i];
            str = tmp;
        }
        else
        if ( measure(str) == 1 ) {
            string stem = "";
            for ( int i=0; i<str.length()-1; i++ )
                stem += str[i];

            if ( !cvc(stem) )
                str = stem;
        }
    }

    if ( str.length() == 1 )
        return str;
    if ( (str[str.length()-1] == 'l')
            && (str[str.length()-2] == 'l') && (measure(str) > 1) )
        if ( measure(str) > 1 ) {
            string tmp = "";
            for ( int i=0; i<str.length()-1; i++ )
                tmp += str[i];
            str = tmp;
        }
        
    return str;
}



string StemmingProcessor::stripSuffixes( string str ) {

    str = step1( str );
    if ( str.length() >= 1 )
        str = step2( str );
    if ( str.length() >= 1 )
        str = step3( str );
    if ( str.length() >= 1 )
        str = step4( str );
    if ( str.length() >= 1 )
        str = step5( str );

    return str;
}

string StemmingProcessor::stripAffixes( string str ) {

    str = ToLowerCase(str);
    str = Clean(str);

    if ((str.compare("") != 0) && (str.length() > 2)) {
        str = stripPrefixes(str);

        if (str.compare("") != 0)
            str = stripSuffixes(str);
    }

    return str;
}

