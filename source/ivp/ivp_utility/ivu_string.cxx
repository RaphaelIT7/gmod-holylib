// Copyright (C) Ipion Software GmbH 1999-2000. All rights reserved.

#include <ivp_physics.hxx>

#include <cstdlib>
#include <cstdarg>
#include <cstring>

#include <cctype>

#if !defined(__MWERKS__) || !defined(__POWERPC__)
#ifdef OSX
#include <malloc/malloc.h>
#else
#include <malloc.h>
#endif
#endif
#ifdef WIN32
#	ifndef WIN32_LEAN_AND_MEAN
#		define NOMINMAX
#		define WIN32_LEAN_AND_MEAN
#	endif
#	include <Windows.h>
#endif

void P_String::uppercase(char *str)
{
	char c;
	while( (c=*str) )	{
		if ( (c<='z') && (c>='a')) *str = c - 'a' + 'A';
		str++;
	}
}

const char *P_String::find_string(const char *str, const char *key, int upper_case)
{
/* checks for a substring in another string
   upper_case = 0	->exact match
              = 1	-> a==A
              = 2	-> a==A && a==?
*/
    if ( str == NULL ) return NULL;
    
	const char  *p1, *p2;
	char b;
	switch (upper_case) {
	case 1:
		for (p1 = str, p2 = key; *p1;) {
			if (!(b = *p2)) {
				return (char *)str;
			} else {
				if (toupper(*p1) == toupper(b)) {
					p1++;
					p2++;
				} else {
					p2 = key;
					p1 = (++str);
				}
			}
		}
		if (!*p2) return (char *)str;
		break;
	case 0:
		for (p1 = str, p2 = key; *p1;) {
			if (!(b = *p2)) {
				return (char *)str;
			} else {
				if (b == *p1) {
					p1++;
					p2++;
				} else {
					p2 = key;
					p1 = (++str);
				}
			}
		}
		if (!*p2)	return (char *)str;
		break;
	case 2:
		for (p1 = str, p2 = key; *p1;) {
			if (!(b = *p2)) {
				return (char *)str;
			} else {
				if (b == *p1 || (b=='?')) {
					p1++;
					p2++;
				} else {
					p2 = key;
					p1 = (++str);
				}
			}
		}
		if (!*p2)	return (char *)str;
		break;
	default:
		for (p1 = str, p2 = key; *p1;) {
			if (!(b = *p2)) {
				return (char *)str;
			} else {
				if (toupper(*p1) == toupper(b) || (b=='?') ) {
					p1++;
					p2++;
				} else {
					p2 = key;
					p1 = (++str);
				}
			}
		}
		if (!*p2) return (char *)str;
		break;
	}
	return 0;
}


int P_String::string_cmp(const char *str,const char *search,IVP_BOOL upper_case)
	/*	*	Wildcard in search string */
	/*	?	any	Charakter	*/
	/*	if uppercase	change all letters to uppercase */
	/*	returns 0 if strings are equal -1 or +1 if left strings is
		less/greater than right string */
	{
	const char *p1,*p2;
	char a,b,*d;
	IVP_INT32 i;
	char fsbuf[256];

	p1 = str;
	if (!p1) return -1;
	p2 = search;
	while (1) {
		a = *p1;b=*p2;
		if (b == '*')	{
			if (!p2[1]) return 0;	/* last character is wildcard */
			i = 0;
			d = fsbuf;
			for (p2++;(b=*p2)&&(b!='*');){
				*(d++) = b;
				p2++;
				i++;
				if (i > 250) break;
			}
			if (*p2 != '*' ) {
				p1 += strlen(p1)-i;	/* check the end of the string */
				if (p1 < str) return -1;	/* neither less or greater */
				p2 -= i;
			}else{
				*d=0;
				p1 = find_string(p1,fsbuf,upper_case+2);
				if (!p1) return -1;
				p1 += i;
			}
			continue;
		}else{
			if (!a) return b;
			if (!b) return a;
			if (b != '?')	{
				if (upper_case) {
					if (toupper(a) !=toupper(b) ) return 1;
				}else{
					if (a!=b) return 1;
				}
			}
		}
		p1++;
		p2++;
	}
	return 0;		/* just to satisfy the compiler */
}

char *gbs_add_path(char *path,char *name)
	{
	hk_intp i,len,found;
	char *erg;
	if (!name) return name;
	if (!path) {
		return 0;
	}
	if (*name == '/') return name;
	found =0;
	len = strlen(path);
	for (i=0;i<len;i++) {
		if (path[i] == '/') found = i+1;
	}
	len = found + strlen(name);
	erg = (char *)p_calloc(sizeof(char),(size_t)(len +1));
	for (i=0;i<found;i++) {
		erg[i] = path[i];
	}
	for (i=found;i<len;i++){
		erg[i] = name[i-found];
	}
	return erg;
}

/********************************************************************************************
					String Replace
********************************************************************************************/



char *GBS_remove_escape(char *com)	/* \ is the escape charakter */
{
	char *result,*s,*d;
	char	ch;

	s = d = result = p_strdup(com);
	while ( (ch = *(s++)) ){
		switch (ch) {
			case '\\':
				ch = *(s++); if (!ch) { s--; break; };
				switch (ch) {
					case 'n':	*(d++) = '\n';break;
					case 't':	*(d++) = '\t';break;
					case '0':	*(d++) = '\0';break;
					default:	*(d++) = ch;break;
				}
				break;
			default:
					*(d++) = ch;
		}
	}
	*d = 0;
	return result;
}

char *p_strdup(const char *s)
{
    // kann auch NULLen
    if(s){
	size_t len = strlen(s)+1;
	char *s2 = (char *)p_malloc(len);
	memcpy(s2,(char *)s,len); 
	return s2;
    }else{
	return NULL;
    }
}

#define MAX_MAKE_STRING_LEN 10000

char *p_make_string_fast(const char *templat, ...)
{
    // returns an allocated string with format like ivp_message
    // NULL-Strings and empty strings allowed
    // no check for overflow
    
    if(!templat) return NULL;
    
    char buffer[MAX_MAKE_STRING_LEN];
    va_list parg;
    memset(buffer,0,std::min(1000, MAX_MAKE_STRING_LEN)); // nur bei sparc-debugging
    va_start(parg,templat);	 //-V2019
    vsnprintf(buffer,std::size(buffer),templat,parg);
    va_end(parg);
    return p_strdup(buffer);
}

char *p_make_string(const char *templat, ...)
{
    // returns an allocated string with format like ivp_message
    // NULL-Strings and empty strings allowed
    // LINUX: check for overflow
    
    if(!templat) return NULL;
    
    char buffer[MAX_MAKE_STRING_LEN];
    va_list parg;
    memset(buffer,0,std::min(1000, MAX_MAKE_STRING_LEN)); // only for sparc-debugging
    va_start(parg,templat); //-V2019
    vsnprintf(buffer, std::size(buffer), templat, parg);
    va_end(parg);
    return p_strdup(buffer);
}

#define MAX_ERROR_BUFFER_LEN 10000
char *p_error_buffer = 0;

IVP_ERROR_STRING p_export_error(const char *templat, ...)
{
    // for general error management... z.B. p_error_message()
    char buffer[MAX_ERROR_BUFFER_LEN];
    char *p = buffer;
    va_list	parg;
    memset(buffer,0, std::min(1000, MAX_ERROR_BUFFER_LEN)); // only for sparc-debugging
    snprintf (buffer, std::size(buffer), "ERROR: ");
    p += strlen(p);
    
    va_start(parg,templat);	 //-V2019
    vsnprintf(buffer,std::size(buffer),templat,parg);
    va_end(parg);

    P_FREE(p_error_buffer);
    p_error_buffer = p_strdup(buffer);
    return p_error_buffer;
}

void ivp_message(const char *fmt, ...)
{
    // for general error management... z.B. p_error_message()
    char buffer_tmp[MAX_ERROR_BUFFER_LEN] = { 'E', 'R', 'R', 'O', 'R', ':', ' ', '\0' };
    va_list	args;

    va_start(args, fmt); //-V2019
    vsnprintf(buffer_tmp, std::size(buffer_tmp), fmt, args);
    va_end(args);

    char buffer_out[MAX_ERROR_BUFFER_LEN];
    snprintf(buffer_out, std::size(buffer_out), "[havok] %s", buffer_tmp);

#ifdef WIN32
    OutputDebugStringA(buffer_out);
#else
    fprintf(stderr, "%s", buffer_out);
#endif
}


IVP_ERROR_STRING p_get_error(){
    return p_error_buffer;
}

void p_print_error() {
    char buffer_out[MAX_ERROR_BUFFER_LEN];
    snprintf(buffer_out, std::size(buffer_out), "[havok] %s", p_get_error());

#ifdef WIN32
    OutputDebugStringA(buffer_out);
#else
    fprintf(stderr, "%s", buffer_out);
#endif
}

char *p_read_first_token(FILE *fp){
    static char buffer[1024];
    while( fgets(buffer, 1000, fp) ){
	if(buffer[0] == '#') continue;	//Comment
	char *tok = p_str_tok(buffer, IVP_WHITESPACE);
	if (!tok) continue;
	return tok;
    }
    return 0;
}

char *p_get_string(){
    char *s =  p_str_tok(0,"\n");
    return p_strdup(s);
}

char *p_get_next_token(){
    return  p_str_tok(0,IVP_WHITESPACE);
}

int p_get_num(){
    return atoi(p_str_tok(NULL, IVP_WHITESPACE));
}

IVP_DOUBLE p_get_float(){
    char *str = p_str_tok(NULL, IVP_WHITESPACE);
    if (!str) return 0.0f;
	// dimhotepus: atof -> strtof
    return IVP_DOUBLE(strtof(str, nullptr));
}


ptrdiff_t p_strlen(const char *s)
{
    if(!s || s[0] == 0) return 0;
    return strlen(s);
}

int p_strcmp( const char *s1, const char *s2){
    if (s1 == NULL){
	if (s2 == NULL) return 0;
	return 1;
    }
    if (s2 == NULL) return -1;
    return strcmp(s1,s2);
}

char *p_str_tok(char *a,const char *deli){
	char *temp=strtok(a,(char *)deli);
	// Windows will always append a '0d'; maybe correct this with deli
	if(!temp) return NULL;
	int i=0;
	while(temp[i])
	{
		if(temp[i]==13)
		{
			temp[i]=0;
		}
		i++;
	}
	return temp;
}

IVP_DOUBLE p_atof(const char *s){
    if (!s) return(0.0f);
	// dimhotepus: atof -> strtof
    return IVP_DOUBLE(strtof(s, nullptr));
};

int p_atoi(const char *s){
    if (!s) return(0);
    return atoi(s);
}


#ifdef WIN32
#	include <ctime>

// dimhotepus: long long instead of long for 64 bits.
long long p_get_time(){ // returns seconds since 1970
	time_t t;
	return time(&t);
}


// there's no strcasecmp on Windows !?!

// not sure, whether this implementation is entirely correct
int	strcasecmp(const char *a,const char *b)
{
// chris, Sept 2000
	return stricmp(a,b);
}
#endif
