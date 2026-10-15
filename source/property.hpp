#pragma once

/**
 * @file property.hpp
 * @brief A named, typed view into a region of game memory, with datatype and endianness handling and
 *        a templated accessor for reading the pointed-to value.
 */

#include <cstring>
#include <jaffarCommon/exceptions.hpp>
#include <jaffarCommon/hash.hpp>
#include <string>

namespace jaffarPlus
{

/**
 * @brief A named, typed reference to a value stored at a memory address.
 *
 * A property pairs a name with a raw pointer into game memory, a datatype describing how the bytes
 * at that address are interpreted, and an endianness describing their byte order. Conditions and
 * other engine components read the live value through @ref getValue.
 */
class Property
{
public:
  /// @brief The interpretation of the bytes at the property's memory address.
  enum datatype_t
  {
    dt_uint8,   ///< Unsigned 8-bit integer (config datatype "UINT8").
    dt_uint16,  ///< Unsigned 16-bit integer (config datatype "UINT16").
    dt_uint32,  ///< Unsigned 32-bit integer (config datatype "UINT32").
    dt_uint64,  ///< Unsigned 64-bit integer (config datatype "UINT64").
    dt_int8,    ///< Signed 8-bit integer (config datatype "INT8").
    dt_int16,   ///< Signed 16-bit integer (config datatype "INT16").
    dt_int32,   ///< Signed 32-bit integer (config datatype "INT32").
    dt_int64,   ///< Signed 64-bit integer (config datatype "INT64").
    dt_bool,    ///< Boolean stored in a single byte (config datatype "BOOL").
    dt_float32, ///< Single precision float, 32-bit (config datatype "FLOAT32").
    dt_float64  ///< Double precision float, 64-bit (config datatype "FLOAT64").
  };

  /// @brief The byte order of the value stored at the property's memory address.
  enum endianness_t
  {
    little, ///< Little endian byte order (config endianness "Little").
    big     ///< Big endian byte order (config endianness "Big").
  };

  /// @brief Default construction is disabled; a property requires a name, pointer, datatype and endianness.
  Property() = delete;

  /**
   * @brief Constructs a property describing a value in memory.
   * @param name       Identifying name for the property; also hashed for fast lookup.
   * @param pointer    Pointer to the bytes in game memory this property refers to.
   * @param datatype   How the bytes at @p pointer are interpreted.
   * @param endianness Byte order of the value at @p pointer.
   */
  Property(const std::string& name, void* const pointer, const datatype_t datatype, endianness_t endianness)
      : _name(name), _pointer(pointer), _datatype(datatype), _endianness(endianness), _nameHash(jaffarCommon::hash::hashString(name))
  {
  }

  /**
   * @brief Returns the size in bytes of the property's value.
   * @return The byte size of the property's datatype.
   */
  __INLINE__ size_t getSize() const { return getDatatypeSize(_datatype); }

  /**
   * @brief Maps a configuration endianness string to its @ref endianness_t value.
   * @param endiannessName The endianness token from the config ("Little" or "Big").
   * @return The matching endianness enum value.
   * @throws A logic error if the token is not recognized.
   */
  static __INLINE__ endianness_t parseEndiannessName(const std::string& endiannessName)
  {
    if (endiannessName == "Little") return endianness_t::little;
    if (endiannessName == "Big") return endianness_t::big;

    JAFFAR_THROW_LOGIC("Endianness '%s' not recognized.", endiannessName.c_str());
  }

  /**
   * @brief Maps a configuration datatype string to its @ref datatype_t value.
   * @param datatypeName The datatype token from the config (e.g. "UINT8", "INT32", "BOOL", "FLOAT64").
   * @return The matching datatype enum value.
   * @throws A logic error if the token is not recognized.
   */
  static __INLINE__ datatype_t parseDatatypeName(const std::string& datatypeName)
  {
    if (datatypeName == "UINT8") return datatype_t::dt_uint8;
    if (datatypeName == "UINT16") return datatype_t::dt_uint16;
    if (datatypeName == "UINT32") return datatype_t::dt_uint32;
    if (datatypeName == "UINT64") return datatype_t::dt_uint64;
    if (datatypeName == "INT8") return datatype_t::dt_int8;
    if (datatypeName == "INT16") return datatype_t::dt_int16;
    if (datatypeName == "INT32") return datatype_t::dt_int32;
    if (datatypeName == "INT64") return datatype_t::dt_int64;
    if (datatypeName == "BOOL") return datatype_t::dt_bool;
    if (datatypeName == "FLOAT32") return datatype_t::dt_float32;
    if (datatypeName == "FLOAT64") return datatype_t::dt_float64;

    JAFFAR_THROW_LOGIC("Data type '%s' not recognized.", datatypeName.c_str());
  }

  /**
   * @brief Returns the size in bytes of a given datatype.
   * @param datatype The datatype to measure.
   * @return The byte size of @p datatype.
   * @throws A logic error if the datatype is not recognized.
   */
  static __INLINE__ size_t getDatatypeSize(const datatype_t datatype)
  {
    switch (datatype)
    {
      case dt_uint8: return 1;
      case dt_uint16: return 2;
      case dt_uint32: return 4;
      case dt_uint64: return 8;
      case dt_int8: return 1;
      case dt_int16: return 2;
      case dt_int32: return 4;
      case dt_int64: return 8;
      case dt_bool: return 1;
      case dt_float32: return 4;
      case dt_float64: return 8;
    }

    JAFFAR_THROW_LOGIC("Unidentified datatype %d\n", datatype);
  }

  /**
   * @brief Reads the property's value as a long double, whatever its datatype (exact for every
   *        integer type up to 64 bits). Used by conditions whose operands have different datatypes.
   * @return The property's current value.
   */
  __INLINE__ long double getValueAsLongDouble() const
  {
    switch (_datatype)
    {
      case dt_uint8: return getValue<uint8_t>();
      case dt_uint16: return getValue<uint16_t>();
      case dt_uint32: return getValue<uint32_t>();
      case dt_uint64: return getValue<uint64_t>();
      case dt_int8: return getValue<int8_t>();
      case dt_int16: return getValue<int16_t>();
      case dt_int32: return getValue<int32_t>();
      case dt_int64: return getValue<int64_t>();
      case dt_bool: return getValue<bool>() ? 1.0L : 0.0L;
      case dt_float32: return getValue<float>();
      case dt_float64: return getValue<double>();
    }
    return 0.0L;
  }

  /**
   * @brief Tells whether a datatype is an integer type (signed or unsigned, not bool).
   * @param datatype The datatype to test.
   * @return true for the INT and UINT datatypes.
   */
  static __INLINE__ bool isIntegerDatatype(const datatype_t datatype) { return datatype != dt_bool && datatype != dt_float32 && datatype != dt_float64; }

  /**
   * @brief Reads the property's value at its memory address as type @p T, applying endianness conversion.
   *
   * When the property is little endian the bytes are returned as-is. When it is big endian the bytes
   * are reversed into the returned value.
   * @tparam T The type to read the value as; its size must match the property's datatype size.
   * @return The value at the property's pointer, interpreted as @p T.
   * @throws A logic error if sizeof(T) does not match the property's datatype size.
   */
  template <typename T>
  __INLINE__ T getValue() const
  {
    // Otherwise convert to big endian
    const auto size       = getSize();
    const auto bufferSize = sizeof(T);
    if (size != bufferSize) JAFFAR_THROW_LOGIC("Incompatible datatypes while getting value of property '%s'", getName().c_str());

    // If its little endian, return value as is
    if (_endianness == endianness_t::little) return *(T*)_pointer;

    // Converting to big endian value byte by byte
    T          value;
    const auto sBuf = (uint8_t*)_pointer;
    auto       dBuf = (uint8_t*)&value;
    if (size == 1) { dBuf[0] = sBuf[0]; }
    if (size == 2)
    {
      dBuf[0] = sBuf[1];
      dBuf[1] = sBuf[0];
    }
    if (size == 4)
    {
      dBuf[0] = sBuf[3];
      dBuf[1] = sBuf[2];
      dBuf[2] = sBuf[1];
      dBuf[3] = sBuf[0];
    }
    if (size == 8)
    {
      dBuf[0] = sBuf[7];
      dBuf[1] = sBuf[6];
      dBuf[2] = sBuf[5];
      dBuf[3] = sBuf[4];
      dBuf[4] = sBuf[3];
      dBuf[5] = sBuf[2];
      dBuf[6] = sBuf[1];
      dBuf[7] = sBuf[0];
    }
    return value;
  }

  /** @brief Returns the property's datatype. */
  datatype_t getDatatype() const { return _datatype; }
  /** @brief Returns the property's name. */
  std::string getName() const { return _name; }
  /** @brief Returns the hash of the property's name. */
  jaffarCommon::hash::hash_t getNameHash() const { return _nameHash; }
  /** @brief Returns the raw pointer to the property's value in memory. */
  void* getPointer() const { return _pointer; }

private:
  const std::string                _name;       ///< Identifying name of the property.
  void* const                      _pointer;    ///< Pointer to the property's value in game memory.
  const datatype_t                 _datatype;   ///< How the bytes at @ref _pointer are interpreted.
  const endianness_t               _endianness; ///< Byte order of the value at @ref _pointer.
  const jaffarCommon::hash::hash_t _nameHash;   ///< Precomputed hash of @ref _name for fast lookup.
};

} // namespace jaffarPlus
