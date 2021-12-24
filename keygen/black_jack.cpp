/**
	BlackJack SQL plugin project
	(c) 2021 Leonid B. TI-182 f/r

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA

*/

#include <iostream>
#include <string>
#include <vector>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <ctime>
using namespace std;

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


void DecryptKey( const string& in_key, BLACK_KEY* out_key ) {

	unsigned int key;
	uint8_t res;
	char c;
	int res_count = 0, key_counter = 0;
	uint8_t* out_str_data = (uint8_t*)out_key;

	memset( out_key, 0, sizeof(BLACK_KEY) );

	for( int i=0; i<in_key.size(); i++ ) {

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

uint16_t CaclKeyCheckSumm( const BLACK_KEY *key ) {
	uint8_t* ptr = (uint8_t*)key;
	uint16_t result = 0x5555;

	for( size_t i=0; i < (sizeof(BLACK_KEY)-sizeof(uint16_t)); i++ ) {
		result = ( ( result ^ ptr[i] ) + ptr[i] ) ^ ( ptr[i] << ( i & 0xF ) );
	}

	return result;
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

	std::time_t t = std::time(nullptr);
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



string CryptKey( const BLACK_KEY & in_str ) {

	string result;
	unsigned int key;
	uint8_t c;
	uint8_t* in_str_data = (uint8_t*)&in_str;

	for( int i=0; i<sizeof(BLACK_KEY); i++ ) {

		key = ( ( i ^ 0x55 << ( i&7 ) ) ) + i*13;
		key = ( ( key >> 8 ) ^ key ) & 0xFF;

		c = in_str_data[i] ^ key;

		if( i && 0==(i%3)) {
			result.append("-");
		}

		result.push_back( (char)('A' + (c >> 4) ) );
		result.push_back( (char)('A' + (c & 0x0F) ) );
	}

	return result;
}


int main(int argc, char **argv) {

	BLACK_KEY key_data;
	string key;
	int i, check;

	cout << "\nKeyGen v 1.02";

	cout << endl << "Key year:";
	cin >> i;
	key_data.year = i;

	cout << endl << "Key month:";
	cin >> i;
	key_data.month = i;

	cout << endl << "Product ID:";
	cin >> i;
	key_data.product_id = i;

	key_data.header = '$';
	key_data.check = CaclKeyCheckSumm( &key_data );

	key = CryptKey( key_data );

	cout << endl << "KEY: " << key << endl;

	DecryptKey( key, &key_data );

	check = CheckKey( key_data, i );

	cout << endl;

	switch(check) {
		default:			cout << "INVALID";	break;
		case XKEY_VALID: 	cout << "VALID";	break;
		case XKEY_EXPIRED: 	cout << "EXPIRED";	break;
	}

	cout << endl;

	return 0;
}
