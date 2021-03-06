/*
** SYNOPSIS
**   echo "passphrase" | pbkdf2 salt_hex count > 48_byte_hex_key_and_iv
**
** DESCRIPTION
**
** Make the "Password-Based Key Derivation Function v2" function found in
** the openssl library available to the command line, as it is not available
** for use from the "openssl" command.  At the time of writing the "openssl"
** command only encrypts using the older, 'fast' pbkdf1.5 method.
**
** The 'salt_hex' is the salt to be used, as a hexadecimal string. Typically
** this is 8 bytes (64 bit), and is an assigned randomly during encryption.
**
** The 'count' is iteration count used to make the calculation of the key
** from the passphrase longer so as to take 1/2 to 2 seconds to generate.
** This complexity prevents slows down brute force attacks enormously.
**
** The output of the above is a 48 bytes in hexadeximal, which is typically
** used for 32 byte encryption key KEY and a 16 byte IV as needed by
** Crypt-AES-256 (or some other encryption method).
**
** NOTE: While the "openssl" command can accept a hex encoded 'key' and 'iv'
** it only does so on the command line, which is insecure.  As such I
** recommend that the output only be used with API access to the "OpenSSL"
** cryptography libraries.
**
*************
**
** Anthony Thyssen   4 November 2009      A.Thyssen@griffith.edu.au
** AitorATuin        3 February 2018  (whitespace password fix in Tomb) 
**
** Based on a test program "pkcs5.c" found on
**   http://www.mail-archive.com/openssl-users@openssl.org
** which uses openssl to perform PBKDF2 (RFC2898) iteritive (slow) password
** hashing.
**
** Build
**    gcc -o pbkdf2 pbkdf2.c -lcrypto
**
*/
#include <stdio.h>
#include <string.h>

#include <gcrypt.h>

/* block size for password buffer */
#define BLOCK_SIZE 40

/* TODO: move print_hex and hex_to_binary to utils.h, with separate compiling */
void print_hex(unsigned char *buf, int len)
{
	int i;

	for(i=0;i<len;i++)
		printf("%02x", buf[i]);
	printf("\n");
}

int hex_to_binary(unsigned char *buf, char *hex)
{
	int ret;
	int count=0;
	while(*hex) {
		if( hex[1] ) {
			ret = sscanf( hex, "%2x", (unsigned int*) buf++ );
			hex += 2;
		}
		else {
			ret = sscanf( hex++, "%1x", (unsigned int*)buf++ );
		}
		count++;
		if( ret != 1)
			return -1;
	}
	*buf = 0;  // null terminate -- precaution
	return count;
}

void cleanup(char *result, int result_len, char *pass, char *salt, int salt_len) {
	int i;

	//clear and free everything
	if (result) {
		for(i=0; i<result_len;i++)
			result[i]=0;
		free(result);
	}
	if (pass) {
		for(i=0; i<strlen(pass); i++) //blank
			pass[i]=0;
		free(pass);
	}
	if (salt) {
		for(i=0; i<salt_len; i++) //blank
			salt[i]=0;
		free(salt);
	}
}

int main(int argc, char *argv[])
{
	char *pass = NULL;
	unsigned char *salt = NULL;
	int salt_len;                  // salt length in bytes
	int ic=0;                        // iterative count
	int result_len;
	unsigned char *result = NULL;       // result (binary - 32+16 chars)
	int i, pw_len = 0;
	int buff_len = BLOCK_SIZE;

	if ( argc != 4 ) {
		fprintf(stderr, "usage: %s salt count len <passwd >binary_key_iv\n", argv[0]);
		exit(10);
	}

	//TODO: move to base64decode
	salt_len = strlen(argv[1])/2+3;
	salt = calloc(salt_len, sizeof(char));
	salt_len=hex_to_binary(salt, argv[1]);
	if( salt_len <= 0 ) {
		fprintf(stderr, "Error: %s is not a valid salt (it must be a hexadecimal string)\n", argv[1]);
		exit(1);
	}

	if( sscanf(argv[2], "%d", &ic) == 0 || ic<=0) {
		fprintf(stderr, "Error: count must be a positive integer\n");
		exit(1);
	}
	if( sscanf(argv[3], "%d", &result_len) == 0 || result_len<=0) {
		fprintf(stderr, "Error: result_len must be a positive integer\n");
		exit(1);
	}

	/* Read password char by char.
	 *
	 * Doing in this way we make sure that blanks (even null bytes) end up
	 * in the password.
	 *
	 * passwords containing just a bunch of spaces are valid
	 */
	pass = calloc(buff_len, sizeof(char)); 
	char c = getchar();
	while (c != EOF) {
		if (pw_len == buff_len) {
			buff_len *= 2;
			pass = realloc(pass, buff_len);
			if (!pass) {
				fprintf(stderr, "Error allocating memory");
				cleanup(result, result_len, pass, salt, salt_len);
				exit(3);
			}
		}
		pass[pw_len] = c;
		pw_len++;
		c = getchar();
	}
	if (pw_len <= 1) {
		fprintf(stderr, "Error: password is empty\n");
		cleanup(result, result_len, pass, salt, salt_len);
		exit(1);
	}
	pass[pw_len-1] = '\0';

	// PBKDF 2
	result = calloc(result_len, sizeof(unsigned char*));
	if (!gcry_check_version ("1.5.0")) {
		fputs ("libgcrypt version mismatch\n", stderr);
		cleanup(result, result_len, pass, salt, salt_len);
		exit (2);
	}
	/* Allocate a pool of 16k secure memory.  This make the secure memory
	available and also drops privileges where needed.  */
	gcry_control (GCRYCTL_INIT_SECMEM, 16384, 0);
	/* It is now okay to let Libgcrypt complain when there was/is
	a problem with the secure memory. */
	gcry_control (GCRYCTL_RESUME_SECMEM_WARN);
	/* Tell Libgcrypt that initialization has completed. */
	gcry_control (GCRYCTL_INITIALIZATION_FINISHED, 0);

	gcry_kdf_derive(pass, pw_len-1, GCRY_KDF_PBKDF2, GCRY_MD_SHA1, salt, salt_len, ic, result_len, result);
	print_hex(result, result_len);            // Key + IV   (as hex string)

	cleanup(result, result_len, pass, salt, salt_len);

	return(0);
}

/* vim: set noexpandtab ts=4 sw=4: */
