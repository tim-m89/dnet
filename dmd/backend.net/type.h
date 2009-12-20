#ifndef TYPE_H__86F914B4_0129_42C4_B307_DC5B0213210E
#define TYPE_H__86F914B4_0129_42C4_B307_DC5B0213210E
// -*- tab-width: 4; indent-tabs-mode: nil;  -*-
// vim: tabstop=4:softtabstop=4:expandtab:shiftwidth=4
//
// $Id: type.h 320 2009-04-07 06:39:09Z cristiv $
//
// Copyright (c) 2008, 2009 Cristian L. Vlasceanu
// Copyright (c) 2008, 2009 Ionut-Gabriel Burete
//
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
#include <iosfwd>
#include <stdexcept>

#pragma region Forward Decls
struct elem;

struct AggregateDeclaration;
struct Declaration;
struct Expression;
struct IRState;
struct Initializer;
struct VarDeclaration;
struct Loc;             // D source code location
struct Type;            // D front-end type
struct TypeInfoDeclaration;

//backend:
class ArrayType;
class AssocArrayType;
//class ArrayPtrType;
class DelegateType;
class EnumType;
class ObjectType;
class PointerType;
class PrimitiveType;    // MSIL primitive type (see John Gough, p. 23, 65)
class ReferenceType;
class SliceType;
class StringType;
class StructType;
class ValueType;        // MSIL value type
class Variable;
#pragma endregion


struct TYPE             // back-end type
{
    virtual ~TYPE() { }
    virtual const char* name() const = 0;

    virtual ArrayType* isArrayType() { return NULL; }
    virtual AssocArrayType* isAssociativeArrayType() { return NULL; }
    virtual DelegateType* isDelegateType() { return NULL; }
    virtual EnumType* isEnumType() { return NULL; }
    virtual ObjectType* isObjectType() { return NULL; }
    virtual PointerType* isPointerType() { return NULL; }
    virtual PrimitiveType* isPrimitiveType() { return NULL; }
    virtual ReferenceType* isReferenceType() { return NULL; }
    virtual SliceType* isSliceType() { return NULL; }
    virtual StringType* isStringType() { return NULL; }
    virtual StructType* isStructType() { return NULL; }
    virtual ValueType* isValueType() { return NULL; }

    // The corresponding type for const values
    virtual TYPE* getConstType() { return NULL; }

    /// Get suffix for type conversions; 
    /// for example an unsigned int 8 has a "u8" suffix,
    /// as in "conv.u8"
    virtual const char* getSuffix() const { return ""; }

    /// Suffix for operations that access array elements
    /// such as ldelem.u8, etc
    virtual const char* getElemSuffix() const
    {
        return getSuffix();
    }

    virtual void emitConversion(std::wostream&) = 0;

    // generate initializer code for variable
    elem* initVar(IRState&, VarDeclaration&, Expression*, elem*);

    /*****************************/
    static TYPE* Void;
    static TYPE* Object;

    // primitive CLR types
    static TYPE* Int;   // native integer
    static TYPE* Uint;  // native unsigned integer
    static TYPE* Int8;
    static TYPE* Uint8;
    static TYPE* Int16;
    static TYPE* Uint16;
    static TYPE* Int32;
    static TYPE* Uint32;
    static TYPE* Int64;
    static TYPE* Uint64;
    static TYPE* Boolean;
    static TYPE* Char;
    static TYPE* Dchar;
    static TYPE* Wchar;
    static TYPE* FloatingPoint32;
    static TYPE* FloatingPoint64;

    static TYPE* SystemString;
    static TYPE& systemString(Loc&);

private:
    virtual elem* init(IRState&, VarDeclaration&, Expression*, elem*) = 0;
};


TYPE& toILType(Loc&, Type* type);
TYPE& toILType(Loc&, Variable*);
TYPE& toILType(Declaration&);
TYPE& toILType(Variable&);


/**************************************************************/
#pragma region Misc. Helpers
struct block;
elem* newArray(block&, Loc&, const ArrayType&, unsigned size = 0);

bool isImplicit(Initializer&);

template<typename T> inline const char* className(T& decl)
{
    return toILType(decl.loc, decl.type).getSuffix();
}
inline const char* className(const char* name)
{
    return name;
}

const char* className(StructType&);

inline ArrayType& getArrayType(TYPE& type)
{
    ArrayType* aType = type.isArrayType();
    if (!aType)
    {
        throw std::logic_error("array type expected");
    }
    return *aType;
}

inline AssocArrayType& getAssocArrayType(TYPE& type)
{
    AssocArrayType* aaType = type.isAssociativeArrayType();
    if (!aaType)
    {
        throw std::logic_error("associative array type expected");
    }
    return *aaType;
}

bool isValueType(TYPE&);

Variable* loadTypeInfo(TypeInfoDeclaration&);

#pragma endregion Misc. Helpers

#endif // TYPE_H__86F914B4_0129_42C4_B307_DC5B0213210E
