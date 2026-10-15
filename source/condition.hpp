#pragma once

/**
 * @file condition.hpp
 * @brief Boolean comparisons between game properties (or immediate values) used by rules and input
 *        sets to decide when they apply.
 */

#include "property.hpp"
#include <jaffarCommon/bitwise.hpp>

namespace jaffarPlus
{

/**
 * @brief Abstract base for a single boolean comparison.
 *
 * A condition compares two operands (each either a game @ref Property or an immediate value) with a
 * relational/bitwise/modulo operator. Rules and input sets hold lists of conditions that must all
 * evaluate to true (logical AND). The concrete, type-specialized implementation is @ref _vCondition.
 */
class Condition
{
public:
  /// @brief The comparison operator applied between the condition's two operands.
  enum operator_t
  {
    op_equal,            ///< operand1 == operand2 (config operator "==")
    op_not_equal,        ///< operand1 != operand2 (config operator "!=")
    op_greater,          ///< operand1 >  operand2 (config operator ">")
    op_greater_or_equal, ///< operand1 >= operand2 (config operator ">=")
    op_less,             ///< operand1 <  operand2 (config operator "<")
    op_less_or_equal,    ///< operand1 <= operand2 (config operator "<=")
    op_bit_true,         ///< the bit of operand1 at index operand2 is set (config operator "BitTrue")
    op_bit_false,        ///< the bit of operand1 at index operand2 is clear (config operator "BitFalse")
    op_modulo_zero,      ///< operand1 % operand2 == 0 (config operator "%0")
    op_modulo_non_zero   ///< operand1 % operand2 != 0 (config operator "%N")
  };

  /**
   * @brief Constructs a condition with the given operator.
   * @param opType The comparison operator to apply when the condition is evaluated.
   */
  Condition(const operator_t opType) : _opType(opType) {}

  /**
   * @brief Evaluates the condition against the current values of its operands.
   * @return true if the comparison holds, false otherwise.
   */
  virtual __INLINE__ bool evaluate() const = 0;
  virtual ~Condition()                     = default;

  /**
   * @brief Maps a configuration operator string to its @ref operator_t value.
   * @param operation The operator token from the config (e.g. "==", ">=", "BitTrue", "%0").
   * @return The matching operator enum value.
   * @throws A logic error if the token is not a recognized operator.
   */
  static __INLINE__ operator_t getOperatorType(const std::string& operation)
  {
    if (operation == "==") return op_equal;
    if (operation == "!=") return op_not_equal;
    if (operation == ">") return op_greater;
    if (operation == ">=") return op_greater_or_equal;
    if (operation == "<") return op_less;
    if (operation == "<=") return op_less_or_equal;
    if (operation == "%0") return op_modulo_zero;
    if (operation == "%N") return op_modulo_non_zero;
    if (operation == "BitTrue") return op_bit_true;
    if (operation == "BitFalse") return op_bit_false;

    JAFFAR_THROW_LOGIC("[Error] Unrecognized operator: '%s'. Valid operators are: ==, !=, >, >=, <, <=, %%0, %%N, BitTrue, BitFalse\n", operation.c_str());

    return op_equal;
  }

  /**
   * @brief Tells whether an operator is a bit test (BitTrue/BitFalse).
   * @param opType The operator.
   * @return true for the bit-test operators.
   */
  static __INLINE__ bool isBitOperator(const operator_t opType) { return opType == op_bit_true || opType == op_bit_false; }

  /**
   * @brief Tells whether an operator is a modulo test (%0/%N).
   * @param opType The operator.
   * @return true for the modulo operators.
   */
  static __INLINE__ bool isModuloOperator(const operator_t opType) { return opType == op_modulo_zero || opType == op_modulo_non_zero; }

protected:
  /**
   * @brief Tests one bit of a value.
   * @param value The value (its two's-complement bits, for negative numbers).
   * @param bit   The bit index.
   * @return true if bit @p bit of @p value is set; false for an index past 63.
   */
  static __INLINE__ bool isBitSet(const uint64_t value, const uint64_t bit) { return bit < 64 && ((value >> bit) & 1) != 0; }

  /// @brief The comparison operator selected for this condition.
  const operator_t _opType;
};

/**
 * @brief Concrete, type-specialized condition over operands of type @p T.
 *
 * Each operand is either a @ref Property (read live on every evaluation) or a fixed immediate value.
 * When a property pointer is non-null it overrides the corresponding immediate. The operator is
 * resolved once at construction to a function pointer to keep @ref evaluate branch-free.
 *
 * @tparam T The numeric type the operands are compared as.
 */
template <typename T>
class _vCondition : public Condition
{
public:
  /**
   * @brief Builds a typed condition from its operands.
   * @param opType     The comparison operator to apply.
   * @param property1  First operand as a property, or nullptr to use @p immediate1.
   * @param property2  Second operand as a property, or nullptr to use @p immediate2.
   * @param immediate1 First operand's immediate value, used when @p property1 is nullptr.
   * @param immediate2 Second operand's immediate value, used when @p property2 is nullptr.
   */
  _vCondition(const operator_t opType, Property* property1, Property* property2, T immediate1, T immediate2)
      : Condition(opType), _property1(property1), _property2(property2), _immediate1(immediate1), _immediate2(immediate2)
  {
    switch (_opType)
    {
      case op_equal: _opFcPtr = _opEqual; break;
      case op_not_equal: _opFcPtr = _opNotEqual; break;
      case op_greater: _opFcPtr = _opGreater; break;
      case op_greater_or_equal: _opFcPtr = _opGreaterOrEqual; break;
      case op_less: _opFcPtr = _opLess; break;
      case op_less_or_equal: _opFcPtr = _opLessOrEqual; break;
      case op_bit_true: _opFcPtr = _opBitTrue; break;
      case op_bit_false: _opFcPtr = _opBitFalse; break;
      case op_modulo_zero: _opFcPtr = _opModuloZero; break;
      case op_modulo_non_zero: _opFcPtr = _opModuloNonZero; break;
      default: break;
    }
  }

  /**
   * @brief Evaluates the comparison using the operands' current values.
   *
   * For each operand, the live property value is used when a property was supplied; otherwise the
   * immediate value is used.
   * @return The result of applying the resolved operator to the two operands.
   */
  __INLINE__ bool evaluate() const
  {
    T immediate1 = _immediate1;
    T immediate2 = _immediate2;
    if (_property1 != nullptr) immediate1 = _property1->getValue<T>();
    if (_property2 != nullptr) immediate2 = _property2->getValue<T>();
    return _opFcPtr(immediate1, immediate2);
  }

private:
  /// @brief Returns a == b.
  static __INLINE__ bool _opEqual(const T a, const T b) { return a == b; }
  /// @brief Returns a != b.
  static __INLINE__ bool _opNotEqual(const T a, const T b) { return a != b; }
  /// @brief Returns a > b.
  static __INLINE__ bool _opGreater(const T a, const T b) { return a > b; }
  /// @brief Returns a >= b.
  static __INLINE__ bool _opGreaterOrEqual(const T a, const T b) { return a >= b; }
  /// @brief Returns a < b.
  static __INLINE__ bool _opLess(const T a, const T b) { return a < b; }
  /// @brief Returns a <= b.
  static __INLINE__ bool _opLessOrEqual(const T a, const T b) { return a <= b; }
  /// @brief Returns true if the bit of @p a at index @p b is set (false for an index past 63).
  static __INLINE__ bool _opBitTrue(const T a, const T b) { return isBitSet((uint64_t)a, (uint64_t)b); }
  /// @brief Returns true if the bit of @p a at index @p b is clear (true for an index past 63).
  static __INLINE__ bool _opBitFalse(const T a, const T b) { return isBitSet((uint64_t)a, (uint64_t)b) == false; }
  /// @brief Returns true if a % b == 0 (false when b is 0).
  static __INLINE__ bool _opModuloZero(const T a, const T b) { return (uint64_t)b != 0 && (uint64_t)a % (uint64_t)b == 0; }
  /// @brief Returns true if a % b != 0 (false when b is 0).
  static __INLINE__ bool _opModuloNonZero(const T a, const T b) { return (uint64_t)b != 0 && (uint64_t)a % (uint64_t)b > 0; }

  /// @brief Resolved operator implementation, selected once at construction from @ref _opType.
  bool (*_opFcPtr)(const T, const T);

  Property* const _property1;  ///< First operand as a property, or nullptr to use @ref _immediate1.
  Property* const _property2;  ///< Second operand as a property, or nullptr to use @ref _immediate2.
  const T         _immediate1; ///< First operand's immediate value (used when @ref _property1 is null).
  const T         _immediate2; ///< Second operand's immediate value (used when @ref _property2 is null).
};

/**
 * @brief Condition whose operands are compared by value whatever their datatypes: each is read as a
 *        long double (exact for every integer type up to 64 bits).
 *
 * Used where a single operand type would change the comparison: two properties of different datatypes,
 * or an integer property against an immediate that is fractional or outside the property's range (e.g.
 * "HP < 17.99" on a UINT8, which must mean HP <= 17, not HP < 17). The bit and modulo operators work on
 * the operands' integer values.
 */
class _wCondition : public Condition
{
public:
  /**
   * @brief Builds a value-compared condition from its operands.
   * @param opType     The comparison operator to apply.
   * @param property1  First operand (a property).
   * @param property2  Second operand as a property, or nullptr to use @p immediate2.
   * @param immediate2 Second operand's immediate value, used when @p property2 is nullptr.
   */
  _wCondition(const operator_t opType, Property* property1, Property* property2, const long double immediate2)
      : Condition(opType), _property1(property1), _property2(property2), _immediate2(immediate2)
  {
  }

  /**
   * @brief Evaluates the comparison using the operands' current values.
   * @return The result of applying the operator to the two operands' values.
   */
  __INLINE__ bool evaluate() const
  {
    const long double a = _property1->getValueAsLongDouble();
    const long double b = _property2 != nullptr ? _property2->getValueAsLongDouble() : _immediate2;
    switch (_opType)
    {
      case op_equal: return a == b;
      case op_not_equal: return a != b;
      case op_greater: return a > b;
      case op_greater_or_equal: return a >= b;
      case op_less: return a < b;
      case op_less_or_equal: return a <= b;
      case op_bit_true: return isBitSet((uint64_t)(int64_t)a, (uint64_t)(int64_t)b);
      case op_bit_false: return isBitSet((uint64_t)(int64_t)a, (uint64_t)(int64_t)b) == false;
      case op_modulo_zero: return (uint64_t)(int64_t)b != 0 && (uint64_t)(int64_t)a % (uint64_t)(int64_t)b == 0;
      case op_modulo_non_zero: return (uint64_t)(int64_t)b != 0 && (uint64_t)(int64_t)a % (uint64_t)(int64_t)b > 0;
    }
    return false;
  }

private:
  Property* const   _property1;  ///< First operand.
  Property* const   _property2;  ///< Second operand as a property, or nullptr to use @ref _immediate2.
  const long double _immediate2; ///< Second operand's immediate value (used when @ref _property2 is null).
};

} // namespace jaffarPlus