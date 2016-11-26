#ifndef _WGTCC_TAC_H_
#define _WGTCC_TAC_H_

#include <cinttypes>
#include <string>

class Type;
class Object;
class ASTConstant;

/*
 * We donot explicitly generate AST.
 * Instead, TAC is chosen to replace it.
 * The design of TAC here is based on the 'Dragon Book'.
 */

class Operand;
class Variable;
class Constant;
class Temporary;
class Translator;
class LValTranslator;
enum class OperandType;

OperandType ToTACOperandType(const Type* type);

enum class OperandType {
  SIGNED,
  UNSIGNED,
  FLOAT,
  AGGREGATE,
};


class Operand {
public:
  virtual ~Operand() {}

  // The readable representation of operands
  virtual const std::string Repr() const = 0;

  bool IsInteger() const {
    return type_ == OperandType::SIGNED ||
           type_ == OperandType::UNSIGNED;
  }
  bool IsUnsigned()   const { return type_ == OperandType::UNSIGNED; }
  bool IsSigned()     const { return type_ == OperandType::SIGNED; }
  bool IsFloat()      const { return type_ == OperandType::FLOAT; }
  bool IsAggregate()  const { return type_ == OperandType::AGGREGATE; }
  size_t width()      const { return width_; }
  OperandType type()  const { return type_; }
protected:
  Operand(size_t width, OperandType type): width_(width), type_(type) {}

  size_t width_;
  OperandType type_;
};


class Variable: public Operand {
public:
  static Variable* New(const Object* obj);
  virtual ~Variable() {}
  virtual const std::string Repr() const { return name_; }
  ssize_t offset() const { return offset_; }

private:
  Variable(size_t width, OperandType type, const std::string& name)
      : Operand(width, type), name_(name) {}
  Variable(size_t width, OperandType type, const ssize_t offset)
      : Operand(width, type), offset_(offset) {}

  // For code gen
  
  const std::string name_;
  ssize_t offset_;
};


class Constant: public Operand {
public:
  static Constant* New(const ASTConstant* c);
  static Constant* Zero();
  static Constant* One();
  virtual ~Constant() {}
  virtual const std::string Repr() const { return std::to_string(val_); }
  uint64_t val() const { return val_; }

private:
  Constant(size_t width, OperandType type, uint64_t val)
      : Operand(width, type), val_(val) {}

  // For a floating pointer number,
  // the value has been converted
  uint64_t val_;
};


// Mapping to infinite register
class Temporary: public Operand {
public:
  static Temporary* New(const Type* type);
  virtual ~Temporary() {}
  virtual const std::string Repr() const { return "t" + std::to_string(id_); }

private:
  Temporary(size_t width, OperandType type)
      : Operand(width, type), id_(GenId()) {}
  static size_t GenId() {
    static size_t id = 0;
    return ++id;
  }

  size_t id_;
};


/*
 * Each operator can be mapped into a machine instruction.
 * Or, a couple of machine instructions(e.g. POST_INC/POST_DEC).
 */
enum class Operator {
  // Binary
  ADD,      // '+'
  SUB,      // '-'
  MUL,      // '*'
  DIV,      // '/'
  LESS,     // '<'
  GREATER,  // '>'
  EQ,       // '=='
  NE,       // '!='
  LE,       // '<='
  GE,       // '>='
  L_SHIFT,  // '<<'
  R_SHIFT,  // '>>'
  OR,       // '|'
  AND,      // '&'
  XOR,      // '^'
  
  // Assignment
  ASSIGN,   // '='
  // x[n]: desginate variable n bytes after x
  DES_SS_ASSIGN,  // '[]=' ; x[n] = y; Des subscripted assignment
  SRC_SS_ASSIGN,  // '=[]' ; x = y[n]; Src subscripted assignment 
  DEREF_ASSIGN,   // '*='  ; *x = y;

  // Unary
  PRE_INC,  // '++' ; t = ++x
  POST_INC, // '++' ; t = x++
  PRE_DEC,  // '--' ; t = --x
  POST_DEC, // '--' ; t = x--
  PLUS,     // '+'  ; t = +x
  MINUS,    // '-'  ; t = -x
  ADDR,     // '&'  ; x = &y
  DEREF,    // '*'  ; x = *y
  COMPT,    // '~'  ; x = ~y
  NOT,      // '!'  ; x = !y
  CAST,

  // Function
  PARAM,    // e.g. param x1
  CALL,     // e.g. call f, n

  // Jump
  JUMP,         // goto des
  IF,           // if (lhs) goto des
  IF_FALSE,     // if (!lhs) goto des
  //IF_LESS,    // if (lhs < rhs)  goto des
  //IF_GREATER, // if (lhs > rhs)  goto des
  //IF_LE,      // if (lhs <= rhs) goto des
  //IF_GE,      // if (lhs >= rhs) goto des
  //IF_EQ,      // if (lhs == rhs) goto des
  //IF_NE,      // if (lhs != rhs) goto des

  LABEL,  // temporary jump dest
};


class TAC {
public:
  static TAC* NewBinary(Operator op, Operand* des,
                        Operand* lhs, Operand* rhs);
  static TAC* NewUnary(Operator op, Operand* des, Operand* operand);
  static TAC* NewAssign(Operand* des, Operand* src);
  static TAC* NewDesSSAssign(Operand* des, Operand* src, ssize_t offset);
  static TAC* NewSrcSSAssign(Operand* des, Operand* src, ssize_t offset);
  static TAC* NewDerefAssign(Operand* des, Operand* src) {
    return NewUnary(Operator::DEREF_ASSIGN, des, src);
  }
  static TAC* NewJump(TAC* des);
  static TAC* NewIf(Operand* cond, TAC* des);
  static TAC* NewIfFalse(Operand* cond, TAC* des);
  static TAC* NewLabel() {
    return NewBinary(Operator::LABEL, nullptr, nullptr, nullptr);
  }
  ~TAC() {}

private:
  TAC(Operator op, Operand* des=nullptr,
      Operand* lhs=nullptr, Operand* rhs=nullptr)
    : op_(op), des_(des), lhs_(lhs), rhs_(rhs) {}
  TAC(Operator op, Operand* des=nullptr,
      Operand* lhs=nullptr, ssize_t n=0)
    : op_(op), des_(des), lhs_(lhs), n_(n) {}
  TAC(Operator op, Operand* lhs=nullptr, TAC* jump_des=nullptr)
    : op_(op), des_(nullptr), lhs_(lhs), jump_des_(jump_des) {}

  Operator op_; 
  Operand* des_; 
  Operand* lhs_;
  union {
    Operand* rhs_;
    ssize_t n_;
    TAC* jump_des_;
  };
};

#endif