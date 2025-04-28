#include "base64.h"
#include "stdlib.h"
#include "string.h"


char base64_map[] = {'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J', 'K', 'L', 'M', 'N', 'O', 'P',
                     'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X', 'Y', 'Z', 'a', 'b', 'c', 'd', 'e', 'f',
                     'g', 'h', 'i', 'j', 'k', 'l', 'm', 'n', 'o', 'p', 'q', 'r', 's', 't', 'u', 'v',
                     'w', 'x', 'y', 'z', '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', '-', '_'};

/*
* @param str input string to encode
* @param len size of input
* @param mode 0 Base64, 1 Base64URL
* @return Encoded string
*/
char *base64_encrypt(unsigned char *str, size_t len, int mode){
    size_t output_len = ((len + 2) / 3) * 4;
    char *encoded = (char*)malloc(output_len + 1);
    char buf[3];
    int ch = 0, c = 0;

    for(int i = 0; i < len; i++){
        buf[ch++] = str[i];
        if(ch == 3){
            // shift 2 bits right to extract 6 bits and use shifted value to map b64 char
            encoded[c++] = base64_map[buf[0] >> 2];

            // mask with 0x03 (00000011) and shift 4 bits to make room for next part
            // shift buf[1] by 4 bits and add lower bits of buf[0] and upper bits of buf[1] together for 6 bits
            encoded[c++] = base64_map[((buf[0] & 0x03) << 4) + (buf[1] >> 4)];

            // mask 4 bits of buf[1] with 0x0f (00001111) and shift by 2 bits
            // extract upper 2 bits of buf[2] by shifting 6 bits and then add values together
            encoded[c++] = base64_map[((buf[1] & 0x0f) << 2) + (buf[2] >> 6)];

            // mask 2 bits of buf[2] with 0x3f (00111111) and then map value
            encoded[c++] = base64_map[buf[2] & 0x3f];
            ch = 0;
        }
    }

    // check if padding "=" is needed
	// bas64URL doesent use padding
	
    if(ch > 0){
        encoded[c++] = base64_map[buf[0] >> 2];
        if(ch == 1){
            encoded[c++] = base64_map[(buf[0] & 0x03) << 4];
            if(mode == 1) encoded[c++] = '=';
        } else if(ch == 2){
            encoded[c++] = base64_map[((buf[0] & 0x03) << 4) + (buf[1] >> 4)];
            encoded[c++] = base64_map[(buf[1] & 0x0f) << 2];
        }
        if(mode == 1) encoded[c++] = '=';
    }

    encoded[c] = '\0';
    return encoded;
}
