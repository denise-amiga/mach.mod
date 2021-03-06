#include <brl.mod/blitz.mod/blitz_string.h>
#include <brl.mod/blitz.mod/blitz_array.h>
//#include <brl.mod/blitz.mod/blitz_unicode.h>
#include "mach_unicode.h"
#include <stdio.h>

// Taken from bbStringToUpper
BBChar machCharUpper( BBChar c ){
    if( c<181 ){
        return (c>='a' && c<='z') ? (c&~32) : c;
    }else{
        int lo=0, hi=sizeof(machToUpperData)/4-1;
        while( lo<=hi ){
            int mid=(lo+hi)/2;
            if( c<machToUpperData[mid*2] ){
                hi=mid-1;
            }else if( c>machToUpperData[mid*2] ){
                lo=mid+1;
            }else{
                return machToUpperData[mid*2+1];
            }
        }
        return c;
    }
}

// Taken from bbStringToLower
BBChar machCharLower( BBChar c ){
    if( c<192 ){
        return (c>='A' && c<='Z') ? (c|32) : c;
    }else{
        int lo=0, hi=sizeof(machToLowerData)/4-1;
        while( lo<=hi ){
            int mid=(lo+hi)/2;
            if( c<machToLowerData[mid*2] ){
                hi=mid-1;
            }else if( c>machToLowerData[mid*2] ){
                lo=mid+1;
            }else{
                return machToLowerData[mid*2+1];
            }
        }
    }
}

BBString *machStringCapFirst( BBString *str ){
    if( str->length == 0 || (str->buf[0] >= 'A' && str->buf[0] <= 'Z') ){
        return str;
    }else{
        BBString *cap = bbStringNew( str->length );
        cap->buf[0] = machCharUpper( str->buf[0] );
        memcpy( cap->buf + 1, str->buf + 1, (str->length - 1 )*sizeof(BBChar) );
        return cap;
    }
}

int machCharContained( BBChar c, BBChar* list, int len ){
    for( int i=0; i<len; i++ ){
        if( c == list[i] ){ return 1; }
    }
    return 0;
}
BBString *machStringCapWord( BBString *str, BBChar* whitespace, int numspace ){
    BBString *cap = bbStringNew( str->length );
    int i, j;
    for( i=0; i<str->length; i++ ){
        if( i == 0 || machCharContained( str->buf[i-1], whitespace, numspace ) ){
            cap->buf[i] = machCharUpper( str->buf[i] );
        }else{
            cap->buf[i] = str->buf[i];
        }
    }
    return cap;
}

#define FORMAT_TOKEN_ESCAPE -1
#define FORMAT_TOKEN_INVALID -2
BBString *machStringFormat( BBString *str, BBArray *bitsarray, BBChar tokenchar ){
    if( bitsarray==&bbEmptyArray ){
        return &bbEmptyString;
    }else{
        int i, j, k;
        int ontoken = 0;
        int sindex = 0;
        int size = str->length;
        int numbits = bitsarray->scales[0];
        BBString **bits = (BBString**)BBARRAYDATA( bitsarray, 1 );
        
        // determine a maximum number of tokens that will need to be accounted for then allocate memory for them
        int tokenalloc = 0; i = 0; while( i < str->length ){ tokenalloc += (str->buf[i++] == tokenchar); }
        int *tokenposition = malloc( sizeof(int)*tokenalloc );
        int *tokenlength = malloc( sizeof(int)*tokenalloc );
        int *tokenindex = malloc( sizeof(int)*tokenalloc );
        
        // find positions of tokens, their lengths, and the indexes of the bits they refer to
        i = 0; while( i < str->length ){
            BBChar ch = str->buf[i];
            if( (ch == tokenchar) && ((i+1) < str->length) ){
                ch = str->buf[i+1]; tokenposition[ ontoken ] = i; tokenlength[ ontoken ] = 2;
                tokenindex[ ontoken ] = FORMAT_TOKEN_INVALID;
                
                if( ch == tokenchar ){              // %%
                    tokenindex[ ontoken ] = FORMAT_TOKEN_ESCAPE;
                }else if( ch == 's' || ch == 'S' ){ // %s
                    tokenindex[ ontoken ] = sindex++;
                }else if( ch >= '0' && ch <= '9' ){ // %0, %1, %9, etc.
                    tokenindex[ ontoken ] = ch - '0';
                }else if( ch == '[' ){              // %[10], %[42], etc.
                    tokenindex[ ontoken ] = 0; j = i+2;
                    while( j < str->length && str->buf[j] != ']' ){ j++; } // find the closing brace
                    if( j < str->length ){
                        tokenlength[ ontoken ] = j-i;
                        int digit = 1; k = i; while( --j>k ){ tokenindex[ ontoken ] += digit * (str->buf[j]-'0'); digit *= 10; } // turn e.g. "512" into (int)512.
                    }
                }
                
                if( tokenindex[ ontoken ] == FORMAT_TOKEN_ESCAPE ){
                    i += 2; size--; ontoken++;
                }else if( tokenindex[ ontoken ] >= numbits ){
                    i += tokenlength[ ontoken ];
                }else if( tokenindex[ ontoken ] >= 0 ){
                    i += tokenlength[ ontoken ];
                    size += bits[ tokenindex[ ontoken ] ]->length - tokenlength[ ontoken ]; ontoken++;
                }else{
                    i++;
                }
            }else{
                i++;
            }
        }
        
        // build the resulting string
        BBString *result;
        if( ontoken == 0 ){ // found no valid tokens?
            result = str;
        }else if( size <= 0 ){ // empty string?
            result = &bbEmptyString;
        }else{ // all is well, continue as normal
            result = bbStringNew( size );
            BBString *bit;
            int l;
            i = 0; // index of result
            j = 0; // index of str
            k = 0; // current token
            while( k < ontoken ){
                l = tokenposition[k] - j; // length of section from str
                memcpy( result->buf + i, str->buf + j, l*sizeof(BBChar) );
                i += l; j += l + tokenlength[k];
                if( tokenindex[k] >= 0 ){
                    bit = bits[ tokenindex[k] ];
                    memcpy( result->buf + i, bit->buf, bit->length*sizeof(BBChar) );
                    i += bit->length;
                }else if( tokenindex[k] == FORMAT_TOKEN_ESCAPE ){
                    result->buf[i++] = tokenchar;
                }
                k++;
            }
            memcpy( result->buf + i, str->buf + j, (str->length - j)*sizeof(BBChar) );
            return result;
        }
        
        // cleanup
        free( tokenposition );
        free( tokenlength );
        free( tokenindex );
        
        // all done!
        return result;
    }
}

// Mostly the same as the bbStringJoin function in blitz_string.c, except it also accepts a lower and upper bound as arguments
BBString *machStringJoinSub( BBString *delimiter, BBArray *bitsarray, int lower, int upper ){
    if( bitsarray==&bbEmptyArray ){
        return &bbEmptyString;
    }else{        
        BBString **bits = (BBString**)BBARRAYDATA( bitsarray, 1 );
        int size = 0;
        
        BBString **b; int i;
        
        b = bits;
        for( i=lower; i<upper; ++i ){
            BBString *bit=*b++;
            size += bit->length;
        }
        
        size += (upper - lower - 1) * delimiter->length;
        
        int delimsize = delimiter->length*sizeof(BBChar);
        BBString *str = bbStringNew( size );
        BBChar *strbuffer = str->buf;
        
        b = bits;
        for( i=lower; i<upper; ++i ){
            if(i){
                memcpy( strbuffer, delimiter->buf, delimsize );
                strbuffer += delimiter->length;
            }
            BBString *bit = *b++;
            memcpy( strbuffer, bit->buf, bit->length*sizeof(BBChar) );
            strbuffer += bit->length;
        }
        
        return str;
    }
}

// Same as bbStringJoinSub but without caring about a delimiter
BBString *machStringConcat( BBArray *bitsarray, int lower, int upper ){
    if( bitsarray==&bbEmptyArray ){
        return &bbEmptyString;
    }else{        
        BBString **bits = (BBString**)BBARRAYDATA( bitsarray, 1 );
        int size = 0;
        
        BBString **b; int i;
        
        b = bits;
        for( i=lower; i<upper; ++i ){
            BBString *bit=*b++;
            size += bit->length;
        }
        
        BBString *str = bbStringNew( size );
        BBChar *strbuffer = str->buf;
        
        b = bits;
        for( i=lower; i<upper; ++i ){
            BBString *bit = *b++;
            memcpy( strbuffer, bit->buf, bit->length*sizeof(BBChar) );
            strbuffer += bit->length;
        }
        
        return str;
    }
}

BBString *machStringInterleave( BBArray *superarray, int start, int step ){
    if( superarray==&bbEmptyArray ){
        return &bbEmptyString;
    }else{
        int i, j;
        int size = 0;
        int totalbits = 0;
        int onbit = 0;
        int usedbits;
        int numbits;
        int numbitsarrays = superarray->scales[0];
        int *onsubbit = malloc( sizeof(int)*numbitsarrays );
        BBArray **bitsarrays = (BBArray**)BBARRAYDATA( superarray, 1 );
        BBArray **b = bitsarrays;
        
        for( i=0; i<numbitsarrays; ++i ){
            onsubbit[i] = 0;
            BBArray *bitsarray = *b++;
            numbits = bitsarray->scales[0];
            totalbits += numbits;
        }
        
        i = start;
        while(onbit < totalbits){
            j = 0;
            while( onsubbit[i] >= bitsarrays[i]->scales[0] ){
                if( j > numbitsarrays ){ goto counted; }
                i = (i + step) % numbitsarrays;
                j++;
            }
            
            BBString **bits = (BBString**)BBARRAYDATA( bitsarrays[i], 1 );
            BBString *bit = bits[ onsubbit[i] ];
            size += bit->length;
            
            onsubbit[i]++;
            i = (i + step) % numbitsarrays;
            onbit++;
        }
        
        counted:
        usedbits = onbit;
        onbit = 0;
        for( i=0; i<numbitsarrays; ++i ){ onsubbit[i] = 0; }
        
        BBString *str = bbStringNew( size );
        BBChar *strbuffer = str->buf;
        
        i = start;
        while(onbit < usedbits){
            while( onsubbit[i] >= bitsarrays[i]->scales[0] ){
                i = (i + step) % numbitsarrays;
            }
            
            BBString **bits = (BBString**)BBARRAYDATA( bitsarrays[i], 1 );
            BBString *bit = bits[ onsubbit[i] ];
            memcpy( strbuffer, bit->buf, bit->length*sizeof(BBChar) );
            strbuffer += bit->length;
            
            onsubbit[i]++;
            i = (i + step) % numbitsarrays;
            onbit++;
        }
        
        free( onsubbit );
        return str;
    }
}
