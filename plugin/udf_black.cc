/* Copyright (c) 2021 Leonid B. TI-182 f/r

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

/*
**
** After the library is made one must notify mysqld about the new
** functions with the commands:
**
** CREATE FUNCTION blackjack RETURNS STRING SONAME "udf_black.so";
** CREATE FUNCTION wget RETURNS STRING SONAME "udf_black.so";
**
** After this the functions will work exactly like native MySQL functions.
** Functions should be created only once.
**
** The functions can be deleted by:
**
** DROP FUNCTION blackjack;
** DROP FUNCTION wget;
**
** The CREATE FUNCTION and DROP FUNCTION update the func@mysql table. All
** Active function will be reloaded on every restart of server
** (if --skip-grant-tables is not given)
**
**
*/

#include <my_global.h>
#include <my_sys.h>

#include <new>
#include <vector>
#include <algorithm>

#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <ctime>
#include <iostream>
#include <string>
#include <curl/curl.h>


struct BLACK_KEY {
	uint8_t	header;
	unsigned short month;
	unsigned short year;
	unsigned int product_id;
	uint16_t check;
} __attribute__((packed));


#define XKEY_INVALID	0
#define XKEY_EXPIRED	1
#define XKEY_VALID		3



#if defined(MYSQL_SERVER)
#include <m_string.h>		/* To get my_stpcpy() */
#else
/* when compiled as standalone */
#include <string.h>
#define my_stpcpy(a,b) stpcpy(a,b)
#endif

#include <mysql.h>
#include <ctype.h>

#ifdef HAVE_DLOPEN

/* These must be right or mysqld will not find the symbol! */

C_MODE_START;
my_bool blackjack_init(UDF_INIT *initid, UDF_ARGS *args, char *message);
void blackjack_deinit(UDF_INIT *initid);
char *blackjack(UDF_INIT *initid, UDF_ARGS *args, char *result, unsigned long *length, char *is_null, char *error);

my_bool wget_init(UDF_INIT *initid, UDF_ARGS *args, char *message);
void wget_deinit(UDF_INIT *initid);
char *wget(UDF_INIT *initid, UDF_ARGS *args, char *result, unsigned long *length, char *is_null, char *error);

C_MODE_END;


uint16_t CaclKeyCheckSumm( const BLACK_KEY *key ) {
	uint8_t* ptr = (uint8_t*)key;
	uint16_t result = 0x5555;

	for( size_t i=0; i < (sizeof(BLACK_KEY)-sizeof(uint16_t)); i++ ) {
		result = ( ( result ^ ptr[i] ) + ptr[i] ) ^ ( ptr[i] << ( i & 0xF ) );
	}

	return result;
}

void DecryptKey( const std::string& in_key, BLACK_KEY* out_key ) {

	unsigned int key;
	uint8_t res;
	char c;
	size_t res_count = 0, key_counter = 0, i;
	uint8_t* out_str_data = (uint8_t*)out_key;

	memset( out_key, 0, sizeof(BLACK_KEY) );

	for( i=0; i<in_key.size(); i++ ) {

		c = in_key[i];

		if( c < 'A' || c > ('A'+0x0F) ) {
			continue;
		}

		if( !res_count ) {
			res = (c - 'A') <<4;
			res_count++;
			continue;
		}

		res += c - 'A';
		res_count=0;

		key = ( ( key_counter ^ 0x55 << ( key_counter&7 ) ) ) + key_counter*13;
		key = ( ( key >> 8 ) ^ key ) & 0xFF;

		res ^= key;

		out_str_data[key_counter] = res;
		key_counter++;
		if( key_counter >= sizeof(BLACK_KEY) ) {
			break;
		}
	}

}




int CheckKey( const BLACK_KEY& key, unsigned int product_id ) {

	int year, month;

	if( '$' != key.header ) {
		return XKEY_INVALID;
	}

	if( key.check != CaclKeyCheckSumm(&key) ) {
		return XKEY_INVALID;
	}

	if( key.product_id != product_id ) {
		return XKEY_INVALID;
	}

	std::time_t t = std::time(0);
	std::tm *const pTInfo = std::localtime(&t);

	year = 1900 + pTInfo->tm_year;
	month = 1 + pTInfo->tm_mon;

	if( key.year < year ) {
		return XKEY_EXPIRED;
	}

	if( key.year == year ) {
		if( key.month < month ) {
			return XKEY_EXPIRED;
		}
	}

	return XKEY_VALID;
}



/*************************************************************************
** Init function
** Arguments:
** initid	Points to a structure that the init function should fill.
**		This argument is given to all other functions.
**	my_bool maybe_null	1 if function can return NULL
**				Default value is 1 if any of the arguments
**				is declared maybe_null.
**	unsigned int decimals	Number of decimals.
**				Default value is max decimals in any of the
**				arguments.
**	unsigned int max_length  Length of string result.
**				The default value for integer functions is 21
**				The default value for real functions is 13+
**				default number of decimals.
**				The default value for string functions is
**				the longest string argument.
**	char *ptr;		A pointer that the function can use.
**
** args		Points to a structure which contains:
**	unsigned int arg_count		Number of arguments
**	enum Item_result *arg_type	Types for each argument.
**					Types are STRING_RESULT, REAL_RESULT
**					and INT_RESULT.
**	char **args			Pointer to constant arguments.
**					Contains 0 for not constant argument.
**	unsigned long *lengths;		max string length for each argument
**	char *maybe_null		Information of which arguments
**					may be NULL
**
** message	Error message that should be passed to the user on fail.
**		The message buffer is MYSQL_ERRMSG_SIZE big, but one should
**		try to keep the error message less than 80 bytes long!
**
** This function should return 1 if something goes wrong. In this case
** message should contain something usefull!
**************************************************************************/

my_bool blackjack_init(UDF_INIT *initid, UDF_ARGS *args, char *message)
{
	if (args->arg_count != 2  ) {
		strcpy(message,"Wrong arguments count to blackjack. Expected 2");
		return 1;
	}

	if ( args->arg_type[0] != STRING_RESULT || args->arg_type[1] != INT_RESULT ) {
		strcpy(message,"Wrong arguments types to blackjack. Expected <string>, <integer>");
		return 1;
	}

	return 0;
}

/****************************************************************************
** Deinit function. This should free all resources allocated by
** this function.
** Arguments:
** initid	Return value from xxxx_init
****************************************************************************/


void blackjack_deinit(UDF_INIT *initid MY_ATTRIBUTE((unused)))
{
}

/***************************************************************************
** UDF string function.
** Arguments:
** initid	Structure filled by xxx_init
** args		The same structure as to xxx_init. This structure
**		contains values for all parameters.
**		Note that the functions MUST check and convert all
**		to the type it wants!  Null values are represented by
**		a NULL pointer
** result	Possible buffer to save result. At least 255 byte long.
** length	Pointer to length of the above buffer.	In this the function
**		should save the result length
** is_null	If the result is null, one should store 1 here.
** error	If something goes fatally wrong one should store 1 here.
**
** This function should return a pointer to the result string.
** Normally this is 'result' but may also be an alloced string.
***************************************************************************/

/* Character coding array */

char *blackjack(UDF_INIT *initid MY_ATTRIBUTE((unused)),
               UDF_ARGS *args, char *result, unsigned long *length,
               char *is_null, char *error MY_ATTRIBUTE((unused)))
{
	const char *key 		= args->args[0];
	ulonglong product_id 	= *((longlong*) args->args[1]);
	BLACK_KEY key_data;
	int check;

	if (!key) { /* Null argument */
		/* The length is expected to be zero when the argument is NULL. */
		assert(args->lengths[0] == 0);
		*is_null=1;
		return 0;
	}

	DecryptKey( std::string(key), &key_data );
	check = CheckKey( key_data, product_id );

	switch(check) {
		default:			strcpy(result,"INVALID");	break;
		case XKEY_VALID: 	strcpy(result,"VALID");		break;
		case XKEY_EXPIRED: 	strcpy(result,"EXPIRED");	break;
	}

    *is_null=0;
    *length = strlen( result );

    return result;
}


my_bool wget_init(UDF_INIT *initid, UDF_ARGS *args, char *message) {

	if ( args->arg_count != 1 || args->arg_type[0] != STRING_RESULT ) {
		strcpy(message,"Wrong arguments to wget. Expected <string>");
		return 1;
	}

	return 0;
}


void wget_deinit(UDF_INIT *initid MY_ATTRIBUTE((unused))){
}


static size_t wget_write(void *contents, size_t size, size_t nmemb, void *userp)
{
    ((std::string*)userp)->append((char*)contents, size * nmemb);
    return size * nmemb;
}



char *wget(UDF_INIT *initid MY_ATTRIBUTE((unused)),
               UDF_ARGS *args, char *result, unsigned long *length,
               char *is_null, char *error )
{
	const char *url=args->args[0];
	CURL *curl;
	CURLcode res;
	std::string readBuffer;
	char *big_result = 0;

	if (!url) { /* Null argument */
		/* The length is expected to be zero when the argument is NULL. */
		assert(args->lengths[0] == 0);
		*is_null=1;
		return 0;
	}

	curl = curl_easy_init();
	if(curl) {
		curl_easy_setopt(curl, CURLOPT_URL, url );
		curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, wget_write);
		curl_easy_setopt(curl, CURLOPT_WRITEDATA, &readBuffer);
		res = curl_easy_perform(curl);
		curl_easy_cleanup(curl);
	} else {
		*error = 1;
		return 0;
	}

	if(res != CURLE_OK) {
		*error = 1;
		return 0;
	}

	if( !readBuffer.size() ) {
		*is_null=1;
		return 0;
	}

	*length = readBuffer.size();

	if( readBuffer.size() >= *length ) {
		big_result = (char*) malloc( readBuffer.size()+1 );
		if( big_result ) {
			strcpy( big_result,readBuffer.c_str());
			return big_result;
		}

		*error = 1;
		return 0;
	}

	strcpy(result,readBuffer.c_str());
	return result;
}


#endif /* HAVE_DLOPEN */
