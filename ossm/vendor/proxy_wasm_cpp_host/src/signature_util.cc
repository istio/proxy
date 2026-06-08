// Copyright 2021 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "include/proxy-wasm/signature_util.h"

#include <array>
#include <cstring>
#include <memory>

#ifdef PROXY_WASM_VERIFY_WITH_ED25519_PUBKEY
#include <openssl/evp.h>
#endif

#include "include/proxy-wasm/bytecode_util.h"

namespace {

#ifdef PROXY_WASM_VERIFY_WITH_ED25519_PUBKEY

uint8_t hex2dec(const unsigned char c) {
  if (c >= '0' && c <= '9') {
    return c - '0';
  }
  if (c >= 'a' && c <= 'f') {
    return c - 'a' + 10;
  }
  if (c >= 'A' && c <= 'F') {
    return c - 'A' + 10;
  }
  throw std::logic_error{"invalid hex character"};
}

template <size_t N> constexpr std::array<uint8_t, N> hex2pubkey(const char (&hex)[2 * N + 1]) {
  std::array<uint8_t, N> pubkey{};
  for (size_t i = 0; i < pubkey.size(); i++) {
    pubkey[i] = hex2dec(hex[2 * i]) << 4 | hex2dec(hex[2 * i + 1]);
  }
  return pubkey;
}

#endif

} // namespace

namespace {

#ifdef PROXY_WASM_VERIFY_WITH_ED25519_PUBKEY

// Helper function to parse a varint from the payload
bool parseVarint(const char *&pos, const char *end, uint32_t &ret) {
  uint32_t shift = 0;
  uint32_t total = 0;
  uint32_t v;
  char b;
  while (pos < end) {
    if (pos + 1 > end) {
      return false;
    }
    b = *pos++;
    v = (b & 0x7f);
    if (shift == 28 && v > 3) {
      return false;
    }
    total += v << shift;
    if ((b & 0x80) == 0) {
      ret = total;
      return true;
    }
    shift += 7;
    if (shift > 28) {
      return false;
    }
  }
  return false;
}

#endif

} // namespace

namespace proxy_wasm {

bool SignatureUtil::verifySignature(std::string_view bytecode, std::string &message) {

#ifdef PROXY_WASM_VERIFY_WITH_ED25519_PUBKEY

  /*
   * Ed25519 signature generated using https://github.com/wasm-signatures/wasmsign2 0.2.6
   * Format specification: https://github.com/WebAssembly/tool-conventions/blob/main/Signatures.md
   *
   * Format notes:
   * - wasmsign2 0.2.6 DOES include signed_hashes_count (previously thought to omit it)
   * - wasmsign2 0.2.6 includes length fields not in the spec:
   *   - signed_hash_len: length of each SignedHash structure (using varint::put_slice)
   *   - signature_bytes_len: length of each signature's data (using varint::put_slice)
   *
   * Signature verification:
   * - The signature is over a message with domain separation, NOT just the hash
   * - Message format: "wasmsig" + spec_version + content_type + hash_fn + hash
   * - See:
   * https://github.com/wasm-signatures/wasmsign2/blob/0.2.6/src/lib/src/signature/multi.rs#L268-L278
   */

  std::string_view signature_payload;
  if (!BytecodeUtil::getCustomSection(bytecode, "signature", signature_payload)) {
    message = "Failed to parse corrupted Wasm module";
    return false;
  }

  if (signature_payload.empty()) {
    message = "Custom Section \"signature\" not found";
    return false;
  }

  // In wasmsign2 0.2.6, the signature section must be FIRST, not last
  // Check if the signature section is at the beginning (after the WASM header)
  // The signature section should start at bytecode offset 8 (after magic + version)
  const char *sig_section_start = bytecode.data() + 8;

  // Verify the signature section is at the beginning by checking if its custom section (type 0)
  if (sig_section_start >= bytecode.data() + bytecode.size() || *sig_section_start != 0) {
    message = "Custom Section \"signature\" not at the beginning of Wasm module";
    return false;
  }

  // Parse wasmsign2 0.2.6 format:
  // spec_version (byte), content_type (byte), hash_fn (byte),
  // signed_hashes_count (varint), then for each SignedHash:
  //   signed_hash_len (varint), hashes_count (varint), hashes (32 bytes each for SHA-256),
  //   signatures_count (varint), then for each signature:
  //     key_id_len (varint), key_id (bytes), signature_id (byte),
  //     signature_len (varint), signature (bytes)

  const char *pos = signature_payload.data();
  const char *end = signature_payload.data() + signature_payload.size();

  if (pos + 3 > end) {
    message = "Signature payload too short";
    return false;
  }

  uint8_t spec_version = static_cast<uint8_t>(*pos++);
  uint8_t content_type = static_cast<uint8_t>(*pos++);
  uint8_t hash_fn = static_cast<uint8_t>(*pos++);

  if (spec_version != 0x01) {
    message = "Unsupported signature spec version: " + std::to_string(spec_version);
    return false;
  }

  if (content_type != 0x01) {
    message = "Unsupported content type: " + std::to_string(content_type);
    return false;
  }

  if (hash_fn != 0x01) {
    message = "Unsupported hash function: " + std::to_string(hash_fn) + " (only SHA-256 supported)";
    return false;
  }

  // Parse signed_hashes_count
  uint32_t signed_hashes_count = 0;
  if (!parseVarint(pos, end, signed_hashes_count) || signed_hashes_count == 0) {
    message = "Invalid or zero signed_hashes_count";
    return false;
  }

  // For simplicity, we only support single SignedHash verification
  if (signed_hashes_count != 1) {
    message = "Only single SignedHash is supported (found " + std::to_string(signed_hashes_count) +
              " SignedHash entries)";
    return false;
  }

  // Parse signed_hash_len (the length of the SignedHash structure)
  uint32_t signed_hash_len = 0;
  if (!parseVarint(pos, end, signed_hash_len)) {
    message = "Invalid signed_hash_len";
    return false;
  }

  // Now parse the SignedHash structure
  uint32_t hashes_count = 0;
  if (!parseVarint(pos, end, hashes_count) || hashes_count == 0) {
    message = "Invalid or zero hashes_count";
    return false;
  }

  // For simplicity, we only support single-hash verification (no partial verification)
  if (hashes_count != 1) {
    message = "Only single-hash signatures are supported (found " + std::to_string(hashes_count) +
              " hashes)";
    return false;
  }

  if (pos + 32 > end) {
    message = "Signature payload too short for hash";
    return false;
  }

  // Extract the expected hash
  const uint8_t *expected_hash = reinterpret_cast<const uint8_t *>(pos);
  pos += 32;

  uint32_t signatures_count = 0;
  if (!parseVarint(pos, end, signatures_count) || signatures_count == 0) {
    message = "Invalid or zero signatures_count";
    return false;
  }

  // We only verify the first signature
  // wasmsign2 0.2.6 includes a signature_bytes_len field before each signature
  uint32_t signature_bytes_len = 0;
  if (!parseVarint(pos, end, signature_bytes_len)) {
    message = "Invalid signature_bytes_len";
    return false;
  }

  uint32_t key_id_len = 0;
  if (!parseVarint(pos, end, key_id_len)) {
    message = "Invalid key_id_len";
    return false;
  }

  // Skip the key_id
  if (key_id_len > 0) {
    if (pos + key_id_len > end) {
      message = "Signature payload too short for key_id";
      return false;
    }
    pos += key_id_len;
  }

  if (pos + 1 > end) {
    message = "Signature payload too short for signature_id";
    return false;
  }

  uint8_t signature_id = static_cast<uint8_t>(*pos++);
  if (signature_id != 0x01) {
    message = "Unsupported signature algorithm: " + std::to_string(signature_id) +
              " (only Ed25519 supported)";
    return false;
  }

  uint32_t signature_len = 0;
  if (!parseVarint(pos, end, signature_len)) {
    message = "Invalid signature_len";
    return false;
  }

  if (signature_len != 64) {
    message =
        "Invalid Ed25519 signature length: " + std::to_string(signature_len) + " (expected 64)";
    return false;
  }

  if (pos + signature_len > end) {
    message = "Signature payload too short for signature data";
    return false;
  }

  const uint8_t *signature = reinterpret_cast<const uint8_t *>(pos);

  // Compute the hash of the module content (everything after the signature section)
  // We need to find where the signature section ends in the original bytecode
  // The signature section structure in WASM is:
  //   section_type (1 byte) + section_len (varint) + name_len (varint) + name + payload

  // Find the end of the signature section
  const char *sig_section_pos = sig_section_start;
  sig_section_pos++; // skip section type (0)

  uint32_t section_len = 0;
  if (!parseVarint(sig_section_pos, bytecode.data() + bytecode.size(), section_len)) {
    message = "Failed to parse signature section length";
    return false;
  }

  uint32_t name_len = 0;
  const char *section_data_start = sig_section_pos;
  if (!parseVarint(sig_section_pos, bytecode.data() + bytecode.size(), name_len)) {
    message = "Failed to parse signature section name length";
    return false;
  }

  // The content to hash starts after the signature section
  const char *content_start = section_data_start + section_len;
  size_t content_len = bytecode.size() - (content_start - bytecode.data());

  // Compute SHA-256 hash of the content
  EVP_MD_CTX *hash_ctx = EVP_MD_CTX_new();
  if (hash_ctx == nullptr) {
    message = "Failed to allocate memory for hash context";
    return false;
  }

  uint8_t computed_hash[32]; // SHA-256 produces 32 bytes
  unsigned int hash_len = 0;

  bool hash_ok = (EVP_DigestInit_ex(hash_ctx, EVP_sha256(), nullptr) != 0) &&
                 (EVP_DigestUpdate(hash_ctx, content_start, content_len) != 0) &&
                 (EVP_DigestFinal_ex(hash_ctx, computed_hash, &hash_len) != 0);

  EVP_MD_CTX_free(hash_ctx);

  if (!hash_ok || hash_len != 32) {
    message = "Failed to compute SHA-256 hash";
    return false;
  }

  // Verify the computed hash matches the expected hash
  if (std::memcmp(computed_hash, expected_hash, 32) != 0) {
    message = "Hash mismatch";
    return false;
  }

  // Verify the signature
  // wasmsign2 signs a message that includes domain separation and metadata:
  // "wasmsig" + spec_version + content_type + hash_fn + hash
  // See:
  // https://github.com/wasm-signatures/wasmsign2/blob/0.2.6/src/lib/src/signature/multi.rs#L268-L278
  const char *domain = "wasmsig";
  size_t domain_len = 7;
  size_t msg_len = domain_len + 3 + 32; // domain + 3 bytes (spec/content/hash) + 32 bytes (hash)
  auto signature_msg = std::make_unique<uint8_t[]>(msg_len);

  std::memcpy(signature_msg.get(), domain, domain_len);
  signature_msg[domain_len] = spec_version;
  signature_msg[domain_len + 1] = content_type;
  signature_msg[domain_len + 2] = hash_fn;
  std::memcpy(signature_msg.get() + domain_len + 3, expected_hash, 32);

  static const auto ed25519_pubkey = hex2pubkey<32>(PROXY_WASM_VERIFY_WITH_ED25519_PUBKEY);

  EVP_PKEY *pubkey = EVP_PKEY_new_raw_public_key(EVP_PKEY_ED25519, nullptr, ed25519_pubkey.data(),
                                                 32 /* ED25519_PUBLIC_KEY_LEN */);
  if (pubkey == nullptr) {
    message = "Failed to load the public key";
    return false;
  }

  EVP_MD_CTX *mdctx = EVP_MD_CTX_new();
  if (mdctx == nullptr) {
    message = "Failed to allocate memory for EVP_MD_CTX";
    EVP_PKEY_free(pubkey);
    return false;
  }

  bool ok = (EVP_DigestVerifyInit(mdctx, nullptr, nullptr, nullptr, pubkey) != 0) &&
            (EVP_DigestVerify(mdctx, signature, 64 /* ED25519_SIGNATURE_LEN */, signature_msg.get(),
                              msg_len) != 0);

  EVP_MD_CTX_free(mdctx);
  EVP_PKEY_free(pubkey);

  if (!ok) {
    message = "Signature mismatch";
    return false;
  }

  message = "Wasm signature OK (Ed25519)";
  return true;

#endif

  return true;
}

} // namespace proxy_wasm
