#ifndef SERIALIZER_H
#define SERIALIZER_H

#include <assert.h>
#include <stdint.h>
#include <string>

#include "ct.pb.h"
#include "types.h"

// A utility class for writing protocol buffer fields in canonical TLS style.
class Serializer {
 public:
  Serializer() {}
  ~Serializer() {}

  // Serialization methods return OK on success,
  // or the first encountered error on failure.
  enum SerializeResult {
    OK,
    INVALID_TYPE,
    EMPTY_CERTIFICATE,
    CERTIFICATE_TOO_LONG,
    CERTIFICATE_CHAIN_TOO_LONG,
    INVALID_HASH_ALGORITHM,
    INVALID_SIGNATURE_ALGORITHM,
    SIGNATURE_TOO_LONG,
    INVALID_HASH_LENGTH,
  };

  static const size_t kMaxCertificateLength;
  static const size_t kMaxCertificateChainLength;
  static const size_t kMaxSignatureLength;

  static size_t PrefixLength(size_t max_length);

  // returns binary data
  std::string SerializedString() const { return output_; }

  static SerializeResult CheckSignedFormat(const ct::CertificateEntry &entry);

  static SerializeResult CheckFormat(const ct::CertificateEntry &entry);

  static SerializeResult SerializeSCTForSigning(uint64_t timestamp, int type,
      const std::string &leaf_certificate, std::string *result);

  static SerializeResult
  SerializeSCTForSigning(const ct::SignedCertificateTimestamp &sct,
                         std::string *result) {
    return SerializeSCTForSigning(sct.timestamp(), sct.entry().type(),
                                  sct.entry().leaf_certificate(), result);
  }

  static SerializeResult SerializeSCTForTree(uint64_t timestamp, int type,
      const std::string &leaf_certificate, std::string *result) {
    return SerializeSCTForSigning(timestamp, type, leaf_certificate, result);
  }

  static SerializeResult
  SerializeSCTForTree(const ct::SignedCertificateTimestamp &sct,
                      std::string *result) {
    return SerializeSCTForTree(sct.timestamp(), sct.entry().type(),
                               sct.entry().leaf_certificate(), result);
  }

  static SerializeResult SerializeSTHForSigning(uint64_t timestamp,
                                                uint64_t tree_size,
                                                const std::string &root_hash,
                                                std::string *result);

  static SerializeResult
  SerializeSTHForSigning(const ct::SignedTreeHead &sth, std::string *result) {
    return SerializeSTHForSigning(sth.timestamp(), sth.tree_size(),
                                  sth.root_hash(), result);
  }

  SerializeResult WriteSCTToken(const ct::SignedCertificateTimestamp &sct);

  static SerializeResult
  SerializeSCTToken(const ct::SignedCertificateTimestamp &sct,
                    std::string *result);

  // TODO(ekasper): tests for these!
  template <class T>
  static std::string SerializeUint(T in, size_t bytes) {
    Serializer serializer;
    serializer.WriteUint(in, bytes);
    return serializer.SerializedString();
  }

  static SerializeResult SerializeDigitallySigned(
      const ct::DigitallySigned &sig, std::string *result);

 private:
  template <class T>
  void WriteUint(T in, size_t bytes) {
    assert(bytes <= sizeof in);
    assert(bytes == sizeof in || in >> (bytes * 8) == 0);
    std::string result;
    for ( ; bytes > 0; --bytes)
      output_.push_back(((in & (static_cast<T>(0xff) << ((bytes - 1) * 8)))
           >> ((bytes - 1) * 8)));
  }

  // Fixed-length byte array.
  void WriteFixedBytes(const std::string &in);

  // Variable-length byte array.
  void WriteVarBytes(const std::string &in, size_t max_length);

  static bool CanSerialize(const repeated_string &in,
                           size_t max_elem_length,
                           size_t max_total_length) {
    return SerializedLength(in, max_elem_length, max_total_length) > 0;
  }

  static size_t SerializedLength(const repeated_string &in,
                                 size_t max_elem_length,
                                 size_t max_total_length);

  SerializeResult WriteDigitallySigned(const ct::DigitallySigned &sig);

  static SerializeResult CheckFormat(const ct::DigitallySigned &sig);

  static SerializeResult CheckFormat(const std::string &cert);

  static SerializeResult CheckFormat(const repeated_string &chain);

  std::string output_;
};

class Deserializer {
 public:
  // We do not make a copy, so input must remain valid.
  Deserializer(const std::string &input);
  ~Deserializer() {}

  enum DeserializeResult {
    OK,
    INPUT_TOO_SHORT,
    INVALID_HASH_ALGORITHM,
    INVALID_SIGNATURE_ALGORITHM,
    INPUT_TOO_LONG,
  };

  bool ReachedEnd() const { return bytes_remaining_ == 0; }

  DeserializeResult ReadSCTToken(ct::SignedCertificateTimestamp *sct);

  static DeserializeResult
  DeserializeSCTToken(const std::string &in,
                      ct::SignedCertificateTimestamp *sct);

  static DeserializeResult
  DeserializeDigitallySigned(const std::string &in, ct::DigitallySigned *sig);

  template<class T>
  static DeserializeResult DeserializeUint(const std::string &in, size_t bytes,
                                           T *result) {
    Deserializer deserializer(in);
    bool res = deserializer.ReadUint(bytes, result);
    if (!res)
      return INPUT_TOO_SHORT;
    if (!deserializer.ReachedEnd())
      return INPUT_TOO_LONG;
    return OK;
  }

 private:
  template<class T>
  bool ReadUint(size_t bytes, T *result) {
    if (bytes_remaining_ < bytes)
      return false;
    T res = 0;
    for (size_t i = 0; i < bytes; ++i) {
      res = (res << 8) | static_cast<unsigned char>(*current_pos_);
      ++current_pos_;
    }

    bytes_remaining_ -= bytes;
    *result = res;
    return true;
  }

  bool ReadFixedBytes(size_t bytes, std::string *result);

  bool ReadVarBytes(size_t max_length, std::string *result);

  DeserializeResult ReadDigitallySigned(ct::DigitallySigned *sig);

  const char *current_pos_;
  size_t bytes_remaining_;
};
#endif