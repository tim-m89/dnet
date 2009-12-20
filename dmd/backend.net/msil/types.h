#ifndef TYPES_H__46A2F664_D17A_4C3C_A3DA_C1C03ED7E783
#define TYPES_H__46A2F664_D17A_4C3C_A3DA_C1C03ED7E783
// -*- tab-width: 4; indent-tabs-mode: nil;  -*-
// vim: tabstop=4:softtabstop=4:expandtab:shiftwidth=4
//
// $Id: types.h 24923 2009-08-06 04:26:28Z unknown $
//
// Copyright (c) 2008, 2009 Cristian L. Vlasceanu
// Copyright (c) 2008 Ionut-Gabriel Burete

/*****
This license governs use of the accompanying software. If you use the software, you
accept this license. If you do not accept the license, do not use the software.

1. Definitions
The terms "reproduce," "reproduction," "derivative works," and "distribution" have the
same meaning here as under U.S. copyright law.
A "contribution" is the original software, or any additions or changes to the software.
A "contributor" is any person that distributes its contribution under this license.
"Licensed patents" are a contributor's patent claims that read directly on its contribution.

2. Grant of Rights
(A) Copyright Grant- Subject to the terms of this license, including the license conditions and limitations in section 3,
each contributor grants you a non-exclusive, worldwide, royalty-free copyright license to reproduce its contribution, 
prepare derivative works of its contribution, and distribute its contribution or any derivative works that you create.
(B) Patent Grant- Subject to the terms of this license, including the license conditions and limitations in section 3, 
each contributor grants you a non-exclusive, worldwide, royalty-free license under its licensed patents to make, have made,
use, sell, offer for sale, import, and/or otherwise dispose of its contribution in the software or derivative works of the 
contribution in the software.

3. Conditions and Limitations
(A) No Trademark License- This license does not grant you rights to use any contributors' name, logo, or trademarks.
(B) If you bring a patent claim against any contributor over patents that you claim are infringed by the software,
your patent license from such contributor to the software ends automatically.
(C) If you distribute any portion of the software, you must retain all copyright, patent, trademark,
and attribution notices that are present in the software.
(D) If you distribute any portion of the software in source code form, you may do so only under this license by 
including a complete copy of this license with your distribution. If you distribute any portion of the software 
in compiled or object code form, you may only do so under a license that complies with this license.
(E) The software is licensed "as-is." You bear the risk of using it. The contributors give no express warranties,
guarantees or conditions. You may have additional consumer rights under your local laws which this license cannot change.
To the extent permitted under your local laws, the contributors exclude the implied warranties of merchantability,
fitness for a particular purpose and non-infringement.
*****/
#include "scope.h"
#include "backend.net/indentable.h"
#include "backend.net/type.h"
#include "backend.net/utils.h"

class Method;


class ValueType : public TYPE
{
    virtual ValueType* isValueType() { return this; }
    virtual void emitConversion(std::wostream&);

    elem* init(IRState&, VarDeclaration&, Expression*, elem*)
    {
        return NULL;
    }
};


class PrimitiveType : public ValueType
{
    PrimitiveType* isPrimitiveType() { return this; }
};


class VoidType : public PrimitiveType
{
public:
    VoidType() { }

    const char* name() const;
    const char* getSuffix() const;
    elem* init(IRState&, VarDeclaration&, Expression*, elem*);
};


///
/// Primitive CLR types
///
class IntegerType : public PrimitiveType
{
    bool isSigned_;
    unsigned numBits_;
    mutable std::string name_;

public:
    IntegerType(bool isSigned, unsigned numBits) 
        : isSigned_(isSigned)
        , numBits_(numBits)
    { }
    const char* name() const;
    const char* getSuffix() const;
    const char* getElemSuffix() const;
    TYPE* getConstType();
    elem* init(IRState&, VarDeclaration&, Expression*, elem*);
};


class NativeIntegerType : public PrimitiveType
{
    bool isSigned_;

public:
    explicit NativeIntegerType(bool isSigned)
        : isSigned_(isSigned)
    { }
    const char* name() const;
    const char* getSuffix() const;
    const char* getElemSuffix() const;
    TYPE* getConstType();
    elem* init(IRState&, VarDeclaration&, Expression*, elem*);
};


class FloatingPointType : public PrimitiveType
{
    unsigned numBits_;
    mutable std::string name_;

public:
    explicit FloatingPointType(unsigned numBits) 
        : numBits_(numBits)
    { }

    const char* name() const;
    const char* getSuffix() const;
    TYPE* getConstType();
    elem* init(IRState&, VarDeclaration&, Expression*, elem*);
};


class BooleanType : public PrimitiveType
{
public:
    BooleanType() { }

    const char* name() const;
    const char* getSuffix() const;
    const char* getElemSuffix() const;
    TYPE* getConstType();
    elem* init(IRState&, VarDeclaration&, Expression*, elem*);
};


class CharType : public PrimitiveType
{
    const unsigned size_; //1, 2 or 4

public:
    explicit CharType(unsigned size = 1) : size_(size) { }

    const char* name() const;
    const char* getSuffix() const;
    const char* getElemSuffix() const;
    TYPE* getConstType();
    elem* init(IRState&, VarDeclaration&, Expression*, elem*);
};


class IdentType : public TYPE
{
    mutable std::string name_;
    TYPE* equivType_;

public:
    explicit IdentType(const char* type, TYPE* equivType = NULL) : equivType_(equivType)
    {
        if (type) name_ = type;
    }
    explicit IdentType(Type& type, TYPE* equivType = NULL);
    ~IdentType();
    const char* name() const;
    elem* init(IRState&, VarDeclaration&, Expression*, elem*);
    const char* getElemSuffix() const { return "ref"; }
    void emitConversion(std::wostream&);
    ArrayType* isArrayType();
};


///<summary>
/// Base for Object types
///</summary>
class ObjectType : public TYPE
{
    Type* type_;

protected:
    mutable std::string name_;
    mutable std::string suffix_;

    /// return the front-end type
    Type* getType() const { return type_; }

public:
    explicit ObjectType(Type* type = NULL);

    ObjectType* isObjectType() { return this; }
    const char* name() const;
    const char* getSuffix() const;
    const char* getElemSuffix() const { return "ref"; }
    elem* init(IRState&, VarDeclaration&, Expression*, elem*);
    void emitConversion(std::wostream&);
    
    ClassDeclaration* getClassDecl();
};


class ArrayTypeBase : public ObjectType
{
    Type* elemType_;

protected:
    explicit ArrayTypeBase(Type& elemType);

public:
    Type& elemType() const { return DEREF(elemType_); }
};


///<summary>
/// Implements static and dynamic array types as IL arrays.
/// For static arrays a non-NULL expression is passed as the 'dim'
/// parameter to the ctor.
/// NOTE: Associative arrays are not handled here.
///</summary>
class ArrayType : public ArrayTypeBase
{
    TYPE& elemTYPE_;    //back-end
    const TYPE* unjaggedElemTYPE_;
    mutable Expressions dim_;

    void initJagged(IRState&, VarDeclaration&) const;

public:
    ArrayType(Loc& loc, Type& elemType, Expression* dim = 0);

    const char* name() const;

    TYPE& elemTYPE() const { return elemTYPE_; }
    elem* init(IRState&, VarDeclaration&, Expression*, elem*);
    elem* init(IRState&, VarDeclaration&, unsigned dim);
    bool initDynamic(block&, Loc&, Variable& array, size_t) const;
    ArrayType* isArrayType() { return this; }
    const char* getSuffix() const;
    const char* getElemSuffix() const { return "ref"; }
    Expression* getSize(unsigned dimension = 0) const;
    unsigned getRank() const { return dim_.dim; }

    //return a slice type with the same element type
    virtual SliceType* getSliceType(Loc&);
};


///<summary>
/// Implements associative array type as System.Collections.Generic.Dictionary
///</summary>
class AssocArrayType : public ObjectType
{
    Loc& loc_;
    Type& elemType_;
    Type& keyType_;

public:
    AssocArrayType(Loc& loc, Type& elemType, Type& keyType);

    const char* name() const;
    const char* argName() const;

    TYPE& elemTYPE() const;
    TYPE& keyTYPE() const;
    Type& elemType() { return elemType_; }
    Type& keyType() { return keyType_; }

    elem* init(IRState&, VarDeclaration&, Expression*, elem*);
    AssocArrayType* isAssociativeArrayType() { return this; }
    const char* getSuffix() const;
};


///<summary>
///System.String type, not to be mistaken with the D string which
///is just an array of bytes
///</summary>
class StringType : public ArrayType
{
public:
    explicit StringType(Loc loc = 0);
    virtual const char* name() const;
    virtual SliceType* getSliceType(Loc&);
    virtual StringType* isStringType() { return this; }
    elem* init(IRState&, VarDeclaration&, Expression*, elem*);
};


class StructType : public ValueType
{
    mutable std::string name_;
    mutable std::string suffix_;
    mutable std::string elemSuffix_;

protected:
    TypeStruct* type_; //front-end type

public:
    explicit StructType(TypeStruct* type) : type_(type)
    {
    }
    const char* name() const;
    const char* getElemSuffix() const;
    const char* getSuffix() const;
    elem* init(IRState&, VarDeclaration&, Expression*, elem*);
    StructType* isStructType() { return this; }
    Type* getType() const { return type_; }
    void emitConversion(std::wostream&);

    StructDeclaration* getStructDecl();
};


///<summary>
/// Enums are implemented in CIL as structs derived off
/// [mscorlib]System.Enum, with static literal fields.
///</summary>
class EnumType : public ValueType
{
    TypeEnum* type_;
    mutable std::string name_;
    mutable std::string suffix_;

public:
    explicit EnumType(TypeEnum* type);
    virtual TYPE* getConstType() { return TYPE::Int32; }
    virtual EnumType* isEnumType() { return this; }
    virtual void emitConversion(std::wostream&);

    //virtual ValueType* isValueType() { return NULL; }

    const char* name() const;
    const char* getSuffix() const;
    //elem* init(IRState&, VarDeclaration&, Expression*, elem*);
};


///<summary>
/// Model a D slice type. In D slices and arrays can be used
/// somewhat interchangeably, hence SliceType inherits off ArrayType.
/// Slice types are mapped onto System.ArraySegment(T).
///</summary>
class SliceType : public ArrayType
{
    Type* type_;

public:
    SliceType(Loc&, Type& elemType);
    virtual SliceType* isSliceType() { return this; }
    //return front-end type
    Type* getType() { return type_; }
    virtual SliceType* getSliceType(Loc&) { return this; }
};


class PointerType : public PrimitiveType
{
    TYPE& pointedType_;
    PointerType* unmanagedType_;
    mutable std::string name_;

    elem* init(IRState&, VarDeclaration&, Expression*, elem*);
    virtual void emitConversion(std::wostream&);

    PointerType* isPointerType() { return this; }

public:
    explicit PointerType(TYPE& pointedType, bool managed = true);
    const char* name() const;
    TYPE& getPointedType() const { return pointedType_; }
    PointerType* getUnmanagedType();
    bool isManaged() const { return unmanagedType_ != this; }
};


class ReferenceType : public PointerType
{
public:
    explicit ReferenceType(TYPE& pointedType) : PointerType(pointedType)
    {
    }
    ArrayType* isArrayType()
    {
        return getPointedType().isArrayType();
    }
    ReferenceType* isReferenceType() { return this; }
};


class DelegateType : public ObjectType, Indentable
{
    Identifier* ident_; // class name
    Method* method_;    // Invoke
    bool external_;
    ClassDeclaration* cd_;
    Scope scope_;

    const char* getSuffix() const;
    unsigned depth() const;

    DelegateType* isDelegateType() { return this; }
    ClassDeclaration* getClassDecl();

public:
    explicit DelegateType(Type* functionType);

    void emit(std::wostream&);
    Method* getMethod();

    void setSuffix(const char* suffix);
    bool isExternal() const { return external_; }
    void setExternal(bool ext = true) { external_ = ext; }
};


int32_t emitFormalArgs(FuncDeclaration*, TypeFunction&, std::wostream&);

Type* getObjectType();
Type* getStringType();

#endif // TYPES_H__46A2F664_D17A_4C3C_A3DA_C1C03ED7E783
