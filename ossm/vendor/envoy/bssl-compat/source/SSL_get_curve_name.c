#include <openssl/ssl.h>

// Extracted from boringssl/ssl/internal.h
struct NamedGroup {
  int nid;
  uint16_t group_id;
  const char name[8], alias[11];
};

// Extracted from boringssl/ssl/ssl_key_share.c
static const struct NamedGroup kNamedGroups[] = {
    {NID_secp224r1, SSL_CURVE_SECP224R1, "P-224", "secp224r1"},
    {NID_X9_62_prime256v1, SSL_CURVE_SECP256R1, "P-256", "prime256v1"},
    {NID_secp384r1, SSL_CURVE_SECP384R1, "P-384", "secp384r1"},
    {NID_secp521r1, SSL_CURVE_SECP521R1, "P-521", "secp521r1"},
    {NID_X25519, SSL_CURVE_X25519, "X25519", "x25519"},
 // {NID_CECPQ2, SSL_CURVE_CECPQ2, "CECPQ2", "CECPQ2"},
};


/*
 * https://github.com/google/boringssl/blob/098695591f3a2665fccef83a3732ecfc99acdcdd/src/include/openssl/ssl.h#L2349
 */
const char *SSL_get_curve_name(uint16_t curve_id) {
  for(int i = 0; i < (sizeof(kNamedGroups) / sizeof(kNamedGroups[0])); i++) {
    if(kNamedGroups[i].group_id == curve_id) {
      return kNamedGroups[i].name;
    }
  }
  return NULL;
}

size_t SSL_get_all_curve_names(const char **out, size_t max_out) {
  size_t nameSize = (sizeof(kNamedGroups) / sizeof(kNamedGroups[0]));
   if(max_out != 0) {
     *out++ = "";
     for(int i = 0; i < nameSize; i++) {
        *out++ = kNamedGroups[i].name;
     }
   }
   return 1+nameSize;
}
