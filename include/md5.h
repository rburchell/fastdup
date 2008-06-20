#ifndef MD5_H
#define MD5_H

#include <stdint.h>

typedef uint8_t md5_byte_t;
typedef uint32_t md5_word_t;

class MD5
{
 private:
	md5_word_t count[2];
	md5_word_t abcd[4];
	md5_byte_t buf[64];
	
	void Process(const md5_byte_t *data);
 public:
	MD5();
	
	void Init();
	void Append(const unsigned char *data, int sz);
	
	unsigned char *Finish();
	void Finish(unsigned char *buf);
	
	char *FinishHex();
	void FinishHex(char *buf);
	
	void Hex(const unsigned char *data, int dsz, char *dst);
};

#endif
