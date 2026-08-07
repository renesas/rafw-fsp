/****************************************************************************
**
** Copyright (C) 2019 Intel Corporation
**
** Permission is hereby granted, free of charge, to any person obtaining a copy
** of this software and associated documentation files (the "Software"), to deal
** in the Software without restriction, including without limitation the rights
** to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
** copies of the Software, and to permit persons to whom the Software is
** furnished to do so, subject to the following conditions:
**
** The above copyright notice and this permission notice shall be included in
** all copies or substantial portions of the Software.
**
** THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
** IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
** FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
** AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
** LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
** OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
** THE SOFTWARE.
**
****************************************************************************/
#ifndef CBOR_H
#define CBOR_H
#ifndef assert
#include <assert.h>
#endif
#include <limits.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include "tinycbor-version.h"
#define TINYCBOR_VERSION            ((TINYCBOR_VERSION_MAJOR << 16) | (TINYCBOR_VERSION_MINOR << 8) | TINYCBOR_VERSION_PATCH)
#ifdef __cplusplus
extern "C" {
#else
#include <stdbool.h>
#endif
#ifndef SIZE_MAX
#  define SIZE_MAX ((size_t)-1)
#endif
#ifndef CBOR_API
#  define CBOR_API
#endif
#ifndef CBOR_PRIVATE_API
#  define CBOR_PRIVATE_API
#endif
#ifndef CBOR_INLINE_API
#  if defined(__cplusplus)
#    define CBOR_INLINE inline
#    define CBOR_INLINE_API inline
#  else
#    define CBOR_INLINE_API static CBOR_INLINE
#    if defined(_MSC_VER)
#      define CBOR_INLINE __inline
#    elif defined(__GNUC__)
#      define CBOR_INLINE __inline__
#    elif defined(__STDC_VERSION__) && __STDC_VERSION__ >= 199901L
#      define CBOR_INLINE inline
#    else
#      define CBOR_INLINE
#    endif
#  endif
#endif

typedef enum {
    CborIntegerType    = 0x00, ///< Major type 0: Unsigned / negative integers
    CborByteStringType = 0x40, ///< Major type 2: Byte strings
    CborTextStringType = 0x60, ///< Major type 3: UTF-8 text strings
    CborArrayType      = 0x80, ///< Major type 4: Arrays (sequence of data items)
    CborMapType        = 0xa0, ///< Major type 5: Maps (key-value pairs)
    CborTagType        = 0xc0, ///< Major type 6: Semantic tags
    CborSimpleType     = 0xe0, ///< Major type 7 (low range): Simple values
    CborBooleanType    = 0xf5, ///< Simple value: Boolean true/false (true=0xf5)
    CborNullType       = 0xf6, ///< Simple value: Null
    CborUndefinedType  = 0xf7, ///< Simple value: Undefined
    CborHalfFloatType  = 0xf9, ///< Half-precision (16-bit) floating point
    CborFloatType      = 0xfa, ///< Single-precision (32-bit) floating point
    CborDoubleType     = 0xfb, ///< Double-precision (64-bit) floating point
    CborInvalidType    = 0xff  ///< Invalid / reserved value
} CborType;

typedef uint64_t CborTag;

typedef enum {
    CborDateTimeStringTag    = 0,     ///< Tag 0: Standardized date/time string (RFC 3339)
    CborUnixTime_tTag        = 1,     ///< Tag 1: Epoch-based time (UNIX time_t)
    CborPositiveBignumTag    = 2,     ///< Tag 2: Positive bignum (arbitrary-size integer)
    CborNegativeBignumTag    = 3,     ///< Tag 3: Negative bignum (arbitrary-size integer)
    CborDecimalTag           = 4,     ///< Tag 4: Decimal fraction (mantissa + exponent)
    CborBigfloatTag          = 5,     ///< Tag 5: Bigfloat (floating-point with exponent)
    CborCOSE_Encrypt0Tag     = 16,    ///< Tag 16: COSE_Encrypt0 structure
    CborCOSE_Mac0Tag         = 17,    ///< Tag 17: COSE_Mac0 structure
    CborCOSE_Sign1Tag        = 18,    ///< Tag 18: COSE_Sign1 structure
    CborExpectedBase64urlTag = 21,    ///< Tag 21: Expected conversion to base64url encoding
    CborExpectedBase64Tag    = 22,    ///< Tag 22: Expected conversion to base64 encoding
    CborExpectedBase16Tag    = 23,    ///< Tag 23: Expected conversion to base16 (hex) encoding
    CborEncodedCborTag       = 24,    ///< Tag 24: Encoded CBOR data item
    CborUrlTag               = 32,    ///< Tag 32: URI/URL string
    CborBase64urlTag         = 33,    ///< Tag 33: Base64url-encoded string
    CborBase64Tag            = 34,    ///< Tag 34: Base64-encoded string
    CborRegularExpressionTag = 35,    ///< Tag 35: Regular expression (PCRE style)
    CborMimeMessageTag       = 36,    ///< Tag 36: MIME message
    CborCOSE_EncryptTag      = 96,    ///< Tag 96: COSE_Encrypt structure
    CborCOSE_MacTag          = 97,    ///< Tag 97: COSE_Mac structure
    CborCOSE_SignTag         = 98,    ///< Tag 98: COSE_Sign structure
    CborSignatureTag         = 55799  ///< Tag 55799: Self-describe CBOR
} CborKnownTags;

#define CborDateTimeStringTag    CborDateTimeStringTag    ///< Tag 0: Standardized date/time string (RFC 3339)
#define CborUnixTime_tTag        CborUnixTime_tTag        ///< Tag 1: Epoch-based time (UNIX time_t)
#define CborPositiveBignumTag    CborPositiveBignumTag    ///< Tag 2: Positive bignum (arbitrary-size integer)
#define CborNegativeBignumTag    CborNegativeBignumTag    ///< Tag 3: Negative bignum (arbitrary-size integer)
#define CborDecimalTag           CborDecimalTag           ///< Tag 4: Decimal fraction (mantissa + exponent)
#define CborBigfloatTag          CborBigfloatTag          ///< Tag 5: Bigfloat (floating-point with exponent)

#define CborCOSE_Encrypt0Tag     CborCOSE_Encrypt0Tag     ///< Tag 16: COSE_Encrypt0 structure (RFC 8152)
#define CborCOSE_Mac0Tag         CborCOSE_Mac0Tag         ///< Tag 17: COSE_Mac0 structure (RFC 8152)
#define CborCOSE_Sign1Tag        CborCOSE_Sign1Tag        ///< Tag 18: COSE_Sign1 structure (RFC 8152)

#define CborExpectedBase64urlTag CborExpectedBase64urlTag ///< Tag 21: Expected conversion to base64url encoding
#define CborExpectedBase64Tag    CborExpectedBase64Tag    ///< Tag 22: Expected conversion to base64 encoding
#define CborExpectedBase16Tag    CborExpectedBase16Tag    ///< Tag 23: Expected conversion to base16 (hex) encoding

#define CborEncodedCborTag       CborEncodedCborTag       ///< Tag 24: Encoded CBOR data item (CBOR-in-CBOR)

#define CborUrlTag               CborUrlTag               ///< Tag 32: URI/URL string
#define CborBase64urlTag         CborBase64urlTag         ///< Tag 33: Base64url-encoded string
#define CborBase64Tag            CborBase64Tag            ///< Tag 34: Base64-encoded string
#define CborRegularExpressionTag CborRegularExpressionTag ///< Tag 35: Regular expression (PCRE-style)
#define CborMimeMessageTag       CborMimeMessageTag       ///< Tag 36: MIME message

#define CborCOSE_EncryptTag      CborCOSE_EncryptTag      ///< Tag 96: COSE_Encrypt structure (RFC 8152)
#define CborCOSE_MacTag          CborCOSE_MacTag          ///< Tag 97: COSE_Mac structure (RFC 8152)
#define CborCOSE_SignTag         CborCOSE_SignTag         ///< Tag 98: COSE_Sign structure (RFC 8152)

#define CborSignatureTag         CborSignatureTag         ///< Tag 55799: Self-describe CBOR

typedef enum CborError {
    CborNoError                        = 0,     ///< No error occurred
    CborUnknownError                   = 1,     ///< Generic unknown error
    CborErrorUnknownLength             = 2,     ///< Indefinite length where definite expected
    CborErrorAdvancePastEOF            = 3,     ///< Attempted to advance past end of buffer
    CborErrorIO                        = 4,     ///< Input/output error during parsing

    CborErrorGarbageAtEnd              = 256,   ///< Extra data found after parsing finished
    CborErrorUnexpectedEOF             = 257,   ///< Unexpected end of input
    CborErrorUnexpectedBreak           = 258,   ///< Unexpected CBOR break code
    CborErrorUnknownType               = 259,   ///< Encountered unknown CBOR type
    CborErrorIllegalType               = 260,   ///< Encountered illegal/invalid CBOR type
    CborErrorIllegalNumber             = 261,   ///< Number encoding not valid
    CborErrorIllegalSimpleType         = 262,   ///< Illegal simple value
    CborErrorUnknownSimpleType         = 512,   ///< Unknown simple value
    CborErrorUnknownTag                = 513,   ///< Encountered unknown tag
    CborErrorInappropriateTagForType   = 514,   ///< Tag does not match data type
    CborErrorDuplicateObjectKeys       = 515,   ///< Map has duplicate keys
    CborErrorInvalidUtf8TextString     = 516,   ///< Text string is not valid UTF-8
    CborErrorExcludedType              = 517,   ///< Encountered excluded CBOR type
    CborErrorExcludedValue             = 518,   ///< Encountered excluded CBOR value
    CborErrorImproperValue             = 519,   ///< Improper/invalid CBOR value
    CborErrorOverlongEncoding          = 520,   ///< Value encoded in non-minimal form
    CborErrorMapKeyNotString           = 521,   ///< Map key was not a string
    CborErrorMapNotSorted              = 522,   ///< Map keys not in required order
    CborErrorMapKeysNotUnique          = 523,   ///< Map keys are not unique

    CborErrorTooManyItems              = 768,   ///< Too many items present
    CborErrorTooFewItems               = 769,   ///< Too few items present

    CborErrorDataTooLarge              = 1024,  ///< Data too large to handle
    CborErrorNestingTooDeep            = 1025,  ///< Too much recursion/nesting
    CborErrorUnsupportedType           = 1026,  ///< Unsupported CBOR type

    CborErrorJsonObjectKeyIsAggregate  = 1280,  ///< JSON object key was array/map
    CborErrorJsonObjectKeyNotString    = 1281,  ///< JSON object key not a string
    CborErrorJsonNotImplemented        = 1282,  ///< JSON conversion not implemented

    CborErrorOutOfMemory = (int) (~0U / 2 + 1), ///< Memory allocation failed
    CborErrorInternalError = (int) (~0U / 2) ///< Internal error (max int)
} CborError;
CBOR_API const char *cbor_error_string(CborError error);
/* Encoder API */
struct CborEncoder
{
    union {
        uint8_t *ptr;
        ptrdiff_t bytes_needed;
    } data;
    const uint8_t *end;
    size_t remaining;
    int flags;
};
typedef struct CborEncoder CborEncoder;
static const size_t CborIndefiniteLength = SIZE_MAX;
#define CBOR_VALUE_MASK             0x1F

CBOR_API CborError cbor_encode_text_string(CborEncoder *enc, const char *txt, size_t len);
CBOR_INLINE_API CborError cbor_encode_text_stringz(CborEncoder *enc, const char *txt)
{
    return cbor_encode_text_string(enc, txt, (size_t)strlen(txt));
}

CBOR_API void      cbor_encoder_init(CborEncoder *enc, uint8_t *buf, size_t len, int flags);
CBOR_API CborError cbor_encode_int(CborEncoder *enc, int64_t v);
CBOR_API CborError cbor_encode_uint(CborEncoder *enc, uint64_t v);
CBOR_API CborError cbor_encode_negative_int(CborEncoder *enc, uint64_t abs_val);
CBOR_API CborError cbor_encode_simple_value(CborEncoder *enc, uint8_t v);
CBOR_API CborError cbor_encode_tag(CborEncoder *enc, CborTag tag);

CBOR_API CborError cbor_encode_byte_string(CborEncoder *enc, const uint8_t *buf, size_t len);
CBOR_API CborError cbor_encode_floating_point(CborEncoder *enc, CborType t, const void *val);
CBOR_INLINE_API CborError cbor_encode_boolean(CborEncoder *encoder, bool value)
{
    int tmp = value - 1;
    return cbor_encode_simple_value(encoder, tmp + (CborBooleanType & CBOR_VALUE_MASK));
}
CBOR_INLINE_API CborError cbor_encode_null(CborEncoder *enc)
{
    return cbor_encode_simple_value(enc, (uint8_t)(CborNullType & CBOR_VALUE_MASK));
}
CBOR_INLINE_API CborError cbor_encode_undefined(CborEncoder *enc)
{
    return cbor_encode_simple_value(enc, (uint8_t)(CborUndefinedType & CBOR_VALUE_MASK));
}

/* Container encoding */
CBOR_API CborError cbor_encoder_close_container_checked(CborEncoder *enc, const CborEncoder *cont);
CBOR_API CborError cbor_encoder_close_container(CborEncoder *enc, const CborEncoder *cont);
CBOR_API CborError cbor_encoder_create_map(CborEncoder *enc, CborEncoder *map, size_t len);
CBOR_API CborError cbor_encoder_create_array(CborEncoder *enc, CborEncoder *arr, size_t len);

CBOR_INLINE_API CborError cbor_encode_float(CborEncoder *enc, float f)
{
    /* Encode single-precision float */
    return cbor_encode_floating_point(enc, CborFloatType, &f);
}

CBOR_INLINE_API CborError cbor_encode_double(CborEncoder *enc, double d)
{
    /* Encode double-precision float */
    return cbor_encode_floating_point(enc, CborDoubleType, &d);
}

CBOR_INLINE_API CborError cbor_encode_half_float(CborEncoder *enc, const void *val)
{
    /* Encode half-precision float */
    return cbor_encode_floating_point(enc, CborHalfFloatType, val);
}

/* Encoder buffer access */
CBOR_INLINE_API uint8_t *cbor_encoder_get_buffer_pointer(const CborEncoder *enc)
{
    return enc->data.ptr;
}

/* keep backward compat alias */
CBOR_INLINE_API uint8_t *_cbor_encoder_get_buffer_pointer(const CborEncoder *enc)
{
    return enc->data.ptr;
}

CBOR_INLINE_API size_t cbor_encoder_get_buffer_size(const CborEncoder *enc, const uint8_t *buf)
{
    return (size_t)(enc->data.ptr - buf);
}

CBOR_INLINE_API size_t cbor_encoder_get_extra_bytes_needed(const CborEncoder *enc)
{
    return (enc->end ? 0u : (size_t)(enc->data.bytes_needed));
}

/* === Parser API === */

/* Parser iteration flags */
typedef enum {
    /* Integer value exceeds 32-bit or 64-bit storage */
    CborIteratorFlag_IntegerValueTooLarge   = 0x01,
    /* Current integer is negative */
    CborIteratorFlag_NegativeInteger        = 0x02,
    /* Alias for readability when iterating string chunks */
    CborIteratorFlag_IteratingStringChunks  = 0x02,
    /* Current item has unknown (indeterminate) length */
    CborIteratorFlag_UnknownLength          = 0x04,
    /* Currently iterating a map container */
    CborIteratorFlag_ContainerIsMap         = 0x20,
    /* Next item in the map is a key (not a value) */
    CborIteratorFlag_NextIsMapKey           = 0x40
} CborParserIteratorFlags;

/* Parser context */
typedef struct {
    const uint8_t *end;
    uint32_t flags;
} CborParser;

/* Iterator over a parsed CBOR value */
typedef struct {
    const CborParser *parser;
    const uint8_t *ptr;
    uint32_t remaining;
    uint16_t extra;
    uint8_t type;
    uint8_t flags;
} CborValue;

/* Internal helpers */
CBOR_PRIVATE_API uint64_t _cbor_value_decode_int64_internal(const CborValue *value);

CBOR_INLINE_API uint64_t cbor_value_extract_int64_helper(const CborValue *value)
{
    return (value->flags & CborIteratorFlag_IntegerValueTooLarge)
                ? _cbor_value_decode_int64_internal(value)
                : value->extra;
}

/* Parser API functions */
CBOR_API CborError cbor_value_validate_basic(const CborValue *val);
CBOR_API CborError cbor_parser_init(const uint8_t *buffer, size_t size, uint32_t flags,
                                    CborParser *parser, CborValue *val);

CBOR_INLINE_API const uint8_t *cbor_value_get_next_byte(const CborValue *val)
{
    return val->ptr;
}

CBOR_INLINE_API bool cbor_value_at_end(const CborValue *val)
{
    bool res = (val->remaining == 0);

    return (res);
}


CBOR_API CborError cbor_value_advance_fixed(CborValue *val);
CBOR_API CborError cbor_value_advance(CborValue *val);

CBOR_INLINE_API bool cbor_value_is_container(const CborValue *val)
{
    bool res = ((val->type == CborArrayType) || (val->type == CborMapType));

    return (res);
}

CBOR_API CborError cbor_value_enter_container(const CborValue *val, CborValue *recursed);
CBOR_API CborError cbor_value_leave_container(CborValue *val, const CborValue *recursed);

/* keep backward compat */
#define _cbor_value_extract_int64_helper(value) cbor_value_extract_int64_helper(value)

/* === Value type checkers === */
CBOR_INLINE_API bool cbor_value_is_valid(const CborValue *value)
{
    return (value && (value->type != CborInvalidType));
}

CBOR_INLINE_API CborType cbor_value_get_type(const CborValue *value)
{
    return (CborType)value->type;
}

CBOR_INLINE_API bool cbor_value_is_null(const CborValue *value)
{
    bool res = (value->type == CborNullType);

    return (res);
}

CBOR_INLINE_API bool cbor_value_is_undefined(const CborValue *value)
{
    return (value->type == CborUndefinedType);
}

CBOR_INLINE_API bool cbor_value_is_boolean(const CborValue *value)
{
    return (value->type == CborBooleanType);
}

CBOR_INLINE_API CborError cbor_value_get_boolean(const CborValue *value, bool *result)
{
    assert(cbor_value_is_boolean(value));
    *result = !!value->extra;
    return CborNoError;
}

CBOR_INLINE_API bool cbor_value_is_simple_type(const CborValue *value)
{
    return (value->type == CborSimpleType);
}

CBOR_INLINE_API CborError cbor_value_get_simple_type(const CborValue *value, uint8_t *result)
{
    assert(cbor_value_is_simple_type(value));
    *result = (uint8_t)value->extra;
    return CborNoError;
}

CBOR_INLINE_API bool cbor_value_is_integer(const CborValue *value)
{
    return (value->type == CborIntegerType);
}
CBOR_INLINE_API bool cbor_value_is_unsigned_integer(const CborValue *value)
{
    return (cbor_value_is_integer(value) && (value->flags & CborIteratorFlag_NegativeInteger) == 0);
}
CBOR_INLINE_API bool cbor_value_is_negative_integer(const CborValue *value)
{
    return (cbor_value_is_integer(value) && (value->flags & CborIteratorFlag_NegativeInteger));
}
CBOR_INLINE_API CborError cbor_value_get_raw_integer(const CborValue *val, uint64_t *res)
{
    /* Retrieve raw integer value (signed or unsigned) */
    assert(cbor_value_is_integer(val));
    *res = cbor_value_extract_int64_helper(val);
    return CborNoError;
}
CBOR_INLINE_API CborError cbor_value_get_uint64(const CborValue *val, uint64_t *res)
{
    /* Retrieve unsigned integer */
    assert(cbor_value_is_unsigned_integer(val));
    *res = cbor_value_extract_int64_helper(val);
    return CborNoError;
}
CBOR_INLINE_API CborError cbor_value_get_int64(const CborValue *val, int64_t *res)
{
    /* Retrieve signed 64-bit integer */
    assert(cbor_value_is_integer(val));
    *res = (int64_t)cbor_value_extract_int64_helper(val);
    if (val->flags & CborIteratorFlag_NegativeInteger)
    {
        *res = -*res;
        *res = *res - 1;
    }

    return CborNoError;
}
CBOR_INLINE_API CborError cbor_value_get_int(const CborValue *val, int *res)
{
    /* Retrieve signed int (platform-dependent size) */
    assert(cbor_value_is_integer(val));
    *res = (int)cbor_value_extract_int64_helper(val);
    if (val->flags & CborIteratorFlag_NegativeInteger)
    {
        *res = -*res;
        *res = *res - 1;
    }

    return CborNoError;
}

/* === Length & Type Checks === */
CBOR_INLINE_API bool cbor_value_is_length_known(const CborValue *val)
{
    bool res = ((val->flags & CborIteratorFlag_UnknownLength) == 0);

    return (res);
}

CBOR_INLINE_API bool cbor_value_is_tag(const CborValue *val)
{
    bool res = (val->type == CborTagType);

    return (res);
}

CBOR_INLINE_API CborError cbor_value_get_tag(const CborValue *val, CborTag *res)
{
    assert(cbor_value_is_tag(val));
    *res = cbor_value_extract_int64_helper(val);
    return CborNoError;
}

CBOR_API CborError cbor_value_skip_tag(CborValue *it);

/* === String Access === */
CBOR_INLINE_API bool cbor_value_is_byte_string(const CborValue *val)
{
    bool res = (val->type == CborByteStringType);

    return (res);
}

CBOR_INLINE_API bool cbor_value_is_text_string(const CborValue *val)
{
    bool res = (val->type == CborTextStringType);

    return (res);
}

CBOR_INLINE_API CborError cbor_value_get_string_length(const CborValue *val, size_t *len)
{
    uint64_t v;

    assert(cbor_value_is_byte_string(val) || cbor_value_is_text_string(val));
    if (!cbor_value_is_length_known(val))
        return CborErrorUnknownLength;
    v = cbor_value_extract_int64_helper(val);
    *len = (size_t)v;
    if (*len != v)
        return CborErrorDataTooLarge;
    return CborNoError;
}

CBOR_PRIVATE_API CborError _cbor_value_copy_string(const CborValue *val, void *buffer,
                                                   size_t *buflen, CborValue *next);
CBOR_INLINE_API CborError cbor_value_copy_text_string(const CborValue *val, char *buffer,
                                                      size_t *buflen, CborValue *next)
{
    assert(cbor_value_is_text_string(val));
    return _cbor_value_copy_string(val, buffer, buflen, next);
}
CBOR_PRIVATE_API CborError _cbor_value_dup_string(const CborValue *va, void **buffer,
                                                  size_t *buflen, CborValue *next);
CBOR_API CborError cbor_value_calculate_string_length(const CborValue *val, size_t *length);

CBOR_INLINE_API CborError cbor_value_copy_byte_string(const CborValue *val, uint8_t *buffer,
                                                      size_t *buflen, CborValue *next)
{
    assert(cbor_value_is_byte_string(val));
    return _cbor_value_copy_string(val, buffer, buflen, next);
}
CBOR_INLINE_API CborError cbor_value_dup_text_string(const CborValue *val, char **buffer,
                                                     size_t *buflen, CborValue *next)
{
    assert(cbor_value_is_text_string(val));
    return _cbor_value_dup_string(val, (void **)buffer, buflen, next);
}
CBOR_INLINE_API CborError cbor_value_dup_byte_string(const CborValue *val, uint8_t **buffer,
                                                     size_t *buflen, CborValue *next)
{
    assert(cbor_value_is_byte_string(val));
    return _cbor_value_dup_string(val, (void **)buffer, buflen, next);
}
CBOR_API CborError cbor_value_text_string_equals(const CborValue *val, const char *string, bool *result);
/* === Array & Map === */
CBOR_INLINE_API bool cbor_value_is_array(const CborValue *val)
{
    bool res = (val->type == CborArrayType);

    return (res);
}

CBOR_INLINE_API bool cbor_value_is_map(const CborValue *val)
{
    bool res = (val->type == CborMapType);

    return (res);
}

CBOR_INLINE_API CborError cbor_value_get_array_length(const CborValue *val, size_t *len)
{
    uint64_t v;
    assert(cbor_value_is_array(val));
    if (!cbor_value_is_length_known(val))
        return CborErrorUnknownLength;
    v = cbor_value_extract_int64_helper(val);
    *len = (size_t)v;
    if (*len != v)
        return CborErrorDataTooLarge;
    else
        return CborNoError;
}

CBOR_INLINE_API CborError cbor_value_get_map_length(const CborValue *val, size_t *len)
{
    uint64_t v;
    assert(cbor_value_is_map(val));
    if (!cbor_value_is_length_known(val))
        return CborErrorUnknownLength;
    v = cbor_value_extract_int64_helper(val);
    *len = (size_t)v;
    if (*len != v)
        return CborErrorDataTooLarge;
    else
        return CborNoError;
}

CBOR_API CborError cbor_value_map_find_value(const CborValue *map, const char *string, CborValue *element);
/* === Floating Point === */
CBOR_INLINE_API bool cbor_value_is_half_float(const CborValue *val)
{
    bool result = (val->type == CborHalfFloatType);

    return (result);
}
CBOR_API CborError cbor_value_get_half_float(const CborValue *val, void *res);
CBOR_INLINE_API bool cbor_value_is_float(const CborValue *val)
{
    bool result = (val->type == CborFloatType);

    return (result);
}
CBOR_INLINE_API CborError cbor_value_get_float(const CborValue *val, float *res)
{
    uint32_t info;
    assert(cbor_value_is_float(val));
    assert(val->flags & CborIteratorFlag_IntegerValueTooLarge);
    info = (uint32_t)_cbor_value_decode_int64_internal(val);
    memcpy(res, &info, sizeof(*res));
    return CborNoError;
}
CBOR_INLINE_API bool cbor_value_is_double(const CborValue *val)
{
    bool res = (val->type == CborDoubleType);

    return res;
}
CBOR_INLINE_API CborError cbor_value_get_double(const CborValue *val, double *res)
{
    uint64_t info;
    assert(cbor_value_is_double(val));
    assert(val->flags & CborIteratorFlag_IntegerValueTooLarge);
    info = _cbor_value_decode_int64_internal(val);
    memcpy(res, &info, sizeof(*res));
    return CborNoError;
}
enum CborValidationFlags {
    /* Minimal validation (no special checks) */
    CborValidateBasic                       = 0,
    /* Require integers use shortest encoding */
    CborValidateShortestIntegrals           = 0x0001,
    /* Require floats use shortest encoding */
    CborValidateShortestFloatingPoint       = 0x0002,
    /* Require all numbers to use shortest encoding */
    CborValidateShortestNumbers             = CborValidateShortestIntegrals | CborValidateShortestFloatingPoint,
    /* Disallow indefinite-length items */
    CborValidateNoIndeterminateLength       = 0x0100,
    /* Require map keys to be sorted (canonical) */
    CborValidateMapIsSorted                 = 0x0200 | CborValidateNoIndeterminateLength,
    /* Validate canonical CBOR formatting */
    CborValidateCanonicalFormat             = 0x0fff,
    /* Ensure all map keys are unique */
    CborValidateMapKeysAreUnique            = 0x1000 | CborValidateMapIsSorted,
    /* Validate that tags are used appropriately */
    CborValidateTagUse                      = 0x2000,
    /* Ensure all text strings are valid UTF-8 */
    CborValidateUtf8                        = 0x4000,
    /* Strict mode validation combining several rules */
    CborValidateStrictMode                  = 0xfff00,
    /* Require map keys to be strings for JSON compatibility */
    CborValidateMapKeysAreString            = 0x100000,
    /* Disallow CBOR "undefined" type */
    CborValidateNoUndefined                 = 0x200000,
    /* Disallow use of CBOR tags */
    CborValidateNoTags                      = 0x400000,
    /* Disallow NaN or Infinity floats */
    CborValidateFiniteFloatingPoint         = 0x800000,
    /* Disallow unknown simple types (standalone) */
    CborValidateNoUnknownSimpleTypesSA      = 0x4000000,
    /* Disallow unknown simple types (combined) */
    CborValidateNoUnknownSimpleTypes        = 0x8000000 | CborValidateNoUnknownSimpleTypesSA,
    /* Disallow unknown tags (standalone) */
    CborValidateNoUnknownTagsSA             = 0x10000000,
    /* Disallow unknown tags (combined) */
    CborValidateNoUnknownTagsSR             = 0x20000000 | CborValidateNoUnknownTagsSA,
    /* Fully disallow unknown tags */
    CborValidateNoUnknownTags               = 0x40000000 | CborValidateNoUnknownTagsSR,
    /* Ensure all input data is consumed */
    CborValidateCompleteData                = (int)0x80000000,
    /* Enable all validation checks (strictest mode) */
    CborValidateStrictest                   = (int)~0U
};
CBOR_API CborError cbor_value_validate(const CborValue *it, uint32_t flags);
/* Flags for pretty-printing CBOR values */
enum CborPrettyFlags {
    /* Show textual encoding indicators (not used) */
    CborPrettyTextualEncodingIndicators     = 0,
    /* Merge consecutive string fragments when printing */
    CborPrettyMergeStringFragments          = 0,
    /* Show indicators for numeric encodings */
    CborPrettyNumericEncodingIndicators     = 0x01,
    /* Indicate indeterminate-length containers */
    CborPrettyIndicateIndeterminateLength   = 0x02,
    /* Alias for CborPrettyIndicateIndeterminateLength */
    CborPrettyIndicateIndetermineLength     = CborPrettyIndicateIndeterminateLength,
    /* Indicate numbers that are overlong encoded */
    CborPrettyIndicateOverlongNumbers       = 0x04,
    /* Show each string fragment separately */
    CborPrettyShowStringFragments           = 0x100,
    /* Default flags used for pretty-printing */
    CborPrettyDefaultFlags                   = CborPrettyIndicateIndeterminateLength
};
typedef CborError (*CborStreamFunction)(void *token, const char *fmt, ...)
#ifdef __GNUC__
#endif
;
CBOR_API CborError cbor_value_to_pretty_stream(CborStreamFunction streamFunction, void *token, CborValue *value, int flags);
#if !defined(__STDC_HOSTED__) || __STDC_HOSTED__-0 == 1
CBOR_API CborError cbor_value_to_pretty_advance_flags(FILE *out, CborValue *value, int flags);
CBOR_API CborError cbor_value_to_pretty_advance(FILE *out, CborValue *value);
CBOR_INLINE_API CborError cbor_value_to_pretty(FILE *out, const CborValue *value)
{
    CborValue copy = *value;
    return cbor_value_to_pretty_advance_flags(out, &copy, CborPrettyDefaultFlags);
}
#endif /* __STDC_HOSTED__ check */
#ifdef __cplusplus
}
#endif
#endif /* CBOR_H */