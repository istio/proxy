// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// ICU-based IDNA converter.

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <ostream>

#include "polyfills/base/check_op.h"
#include <unicode/uidna.h>
#include <unicode/utypes.h>
#include "url/url_canon_icu.h"
#include "url/url_canon_internal.h"  // for _itoa_s

namespace url {

// Use UIDNA, a C pointer to a UTS46/IDNA 2008 handling object opened with
// uidna_openUTS46().
//
// We use UTS46 with BiDiCheck to migrate from IDNA 2003 (with unassigned
// code points allowed) to IDNA 2008 with
// the backward compatibility in mind. What it does:
//
// 1. Use the up-to-date Unicode data.
// 2. Define a case folding/mapping with the up-to-date Unicode data as
//    in IDNA 2003.
// 3. Use transitional mechanism for 4 deviation characters (sharp-s,
//    final sigma, ZWJ and ZWNJ) for now.
// 4. Continue to allow symbols and punctuations.
// 5. Apply new BiDi check rules more permissive than the IDNA 2003 BiDI rules.
// 6. Do not apply STD3 rules
// 7. Do not allow unassigned code points.
//
// It also closely matches what IE 10 does except for the BiDi check (
// http://goo.gl/3XBhqw ).
// See http://http://unicode.org/reports/tr46/ and references therein
// for more details.
UIDNA* GetUIDNA() {
  static UIDNA* uidna = [] {
    UErrorCode err = U_ZERO_ERROR;
    // TODO(jungshik): Change options as different parties (browsers,
    // registrars, search engines) converge toward a consensus.
    UIDNA* value = uidna_openUTS46(UIDNA_CHECK_BIDI, &err);
    if (U_FAILURE(err)) {
      GURL_CHECK(false) << "failed to open UTS46 data with error: "
                   << u_errorName(err)
                   << ". If you see this error message in a test environment "
                   << "your test environment likely lacks the required data "
                   << "tables for libicu. See https://crbug.com/778929.";
      value = nullptr;
    }
    return value;
  }();
  return uidna;
}

// Converts the Unicode input representing a hostname to ASCII using IDN rules.
// The output must be ASCII, but is represented as wide characters.
//
// On success, the output will be filled with the ASCII host name and it will
// return true. Unlike most other canonicalization functions, this assumes that
// the output is empty. The beginning of the host will be at offset 0, and
// the length of the output will be set to the length of the new host name.
//
// On error, this will return false. The output in this case is undefined.
// TODO(jungshik): use UTF-8/ASCII version of nameToASCII.
// Change the function signature and callers accordingly to avoid unnecessary
// conversions in our code. In addition, consider using icu::IDNA's UTF-8/ASCII
// version with StringByteSink. That way, we can avoid C wrappers and additional
// string conversion.
bool IDNToASCII(const char16_t* src, int src_len, CanonOutputW* output) {
  GURL_DCHECK(output->length() == 0);  // Output buffer is assumed empty.

  UIDNA* uidna = GetUIDNA();
  GURL_DCHECK(uidna != nullptr);
  while (true) {
    UErrorCode err = U_ZERO_ERROR;
    UIDNAInfo info = UIDNA_INFO_INITIALIZER;
    int output_length = uidna_nameToASCII(uidna, src, src_len, output->data(),
                                          output->capacity(), &info, &err);

    // Ignore various errors for web compatibility. The options are specified
    // by the WHATWG URL Standard. See
    //  - https://unicode.org/reports/tr46/
    //  - https://url.spec.whatwg.org/#concept-domain-to-ascii
    //    (we set beStrict to false)

    // Disable the "CheckHyphens" option in UTS #46. See
    //  - https://crbug.com/804688
    //  - https://github.com/whatwg/url/issues/267
    info.errors &= ~UIDNA_ERROR_HYPHEN_3_4;
    info.errors &= ~UIDNA_ERROR_LEADING_HYPHEN;
    info.errors &= ~UIDNA_ERROR_TRAILING_HYPHEN;

    // Disable the "VerifyDnsLength" option in UTS #46.
    info.errors &= ~UIDNA_ERROR_EMPTY_LABEL;
    info.errors &= ~UIDNA_ERROR_LABEL_TOO_LONG;
    info.errors &= ~UIDNA_ERROR_DOMAIN_NAME_TOO_LONG;

    if (U_SUCCESS(err) && info.errors == 0) {
      // Per WHATWG URL, it is a failure if the ToASCII output is empty.
      //
      // ICU would usually return UIDNA_ERROR_EMPTY_LABEL in this case, but we
      // want to continue allowing http://abc..def/ while forbidding http:///.
      //
      if (output_length == 0) {
        return false;
      }

      output->set_length(output_length);
      return true;
    }

    if (err != U_BUFFER_OVERFLOW_ERROR || info.errors != 0)
      return false;  // Unknown error, give up.

    // Not enough room in our buffer, expand.
    output->Resize(output_length);
  }
}

}  // namespace url
