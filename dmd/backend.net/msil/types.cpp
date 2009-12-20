// -*- tab-width: 4; indent-tabs-mode: nil;  -*-
// vim: tabstop=4:softtabstop=4:expandtab:shiftwidth=4
//
// $Id: types.cpp 24923 2009-08-06 04:26:28Z unknown $
//
// Copyright (c) 2008, 2009 Cristian L. Vlasceanu

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
#include <stdexcept>
#include <string>
#include "aggregate.h"
#include "declaration.h"
#include "enum.h"
#include "id.h"
#include "init.h"
#include "mtype.h"
#include "backend.net/irstate.h"
#include "backend.net/utils.h"
#include "backend.net/msil/const.h"
#include "backend.net/msil/deref.h"
#include "backend.net/msil/null.h"
#include "backend.net/msil/types.h"
#include "backend.net/msil/variable.h"
#include "frontend.net/externassembly.h"
#include "frontend.net/pragma.h"
using namespace std;


TYPE* TYPE::Void = new VoidType();
TYPE* TYPE::Object = new ObjectType();
TYPE* TYPE::Int = new NativeIntegerType(true);
TYPE* TYPE::Uint = new NativeIntegerType(false);
TYPE* TYPE::Int8 = new IntegerType(true, 8);
TYPE* TYPE::Uint8 = new IntegerType(false, 8);
TYPE* TYPE::Int16 = new IntegerType(true, 16);
TYPE* TYPE::Uint16 = new IntegerType(false, 16);
TYPE* TYPE::Int32 = new IntegerType(true, 32);
TYPE* TYPE::Uint32 = new IntegerType(false, 32);
TYPE* TYPE::Int64 = new IntegerType(true, 64);
TYPE* TYPE::Uint64 = new IntegerType(false, 64);
TYPE* TYPE::Boolean = new BooleanType();
TYPE* TYPE::Char = new CharType();
TYPE* TYPE::Dchar = new CharType(4);
TYPE* TYPE::Wchar = new CharType(2);
TYPE* TYPE::FloatingPoint32 = new FloatingPointType(32);
TYPE* TYPE::FloatingPoint64 = new FloatingPointType(64);

TYPE* TYPE::SystemString = NULL;

static Type* objectType = NULL;
static Type* stringType = NULL;


Type* getObjectType()
{
    if (!objectType)
    {
        objectType = new TypeIdentifier(0, Id::object);
    }
    return objectType;
}


Type* getStringType()
{
    if (!stringType)
    {
        stringType = new TypeIdentifier(0, Lexer::idPool("String"));
    }
    return stringType;
}


Indirect::Indirect(TYPE& t) 
    : t_(t.isPointerType() ? t.isPointerType()->getPointedType() : t)
{
}

/***************************************/
#pragma region VoidType
const char* VoidType::name() const
{
    return "void";
}


const char* VoidType::getSuffix() const
{
    throw logic_error(__FUNCSIG__);
}


elem* VoidType::init(IRState&, VarDeclaration&, Expression*, elem*)
{
    throw logic_error(__FUNCSIG__);
}
#pragma endregion


/***************************************/
#pragma region IntegerType

const char* IntegerType::name() const
{
    if (name_.empty())
    {
        if (!isSigned_)
        {
            name_ = "unsigned ";
        }
        switch (numBits_)
        {
        case 8:  name_ += "int8";  break;
        case 16: name_ += "int16"; break;
        case 32: name_ += "int32"; break;
        case 64: name_ += "int64"; break;
        default:
            throw logic_error("invalid number of bits");
        }
    }
    return name_.c_str();
}


const char* IntegerType::getSuffix() const
{
    switch (numBits_)
    {
    case 8:  return isSigned_ ? "i1" : "u1";
    case 16: return isSigned_ ? "i2" : "u2";
    case 32: return isSigned_ ? "i4" : "u4";
    case 64: return isSigned_ ? "i8" : "u8";
    }
    throw logic_error("invalid number of bits");
}


const char* IntegerType::getElemSuffix() const
{
    switch (numBits_)
    {
    case 8:  return "i1";
    case 16: return "i2";
    case 32: return "i4";
    case 64: return "i8";
    }
    throw logic_error("invalid number of bits");
}


TYPE* IntegerType::getConstType()
{
    return numBits_ <= 32 ? TYPE::Int32 : TYPE::Int64;
}


elem* IntegerType::init(IRState& state, VarDeclaration& decl, Expression* exp, elem*)
{
    if (!decl.init)
    {
        assert(exp == NULL);
        return Const<int>::create(*this, 0, state.getBlock(), decl.loc);
    }
    else
    {
        if (!exp)
        {
            BackEnd::fatalError(decl.init->loc, "NULL initializer expression");
        }
        //initializer expression is expected to be on the stack
        return decl.csym;
    }
}


const char* NativeIntegerType::name() const
{
    if (isSigned_)
    {
        return "native int";
    }
    else
    {
        return "native unsigned int";
    }
}


const char* NativeIntegerType::getSuffix() const
{
    return isSigned_ ? "i" : "u";
}


const char* NativeIntegerType::getElemSuffix() const
{
    return "i";
}


TYPE* NativeIntegerType::getConstType()
{
    return TYPE::Int32;
}


elem* NativeIntegerType::init(IRState& state, VarDeclaration& decl, Expression* exp, elem*)
{
    if (!decl.init)
    {
        assert(exp == NULL);
        return Const<size_t>::create(*this, 0, state.getBlock(), decl.loc);
    }
    else
    {
        if (!exp)
        {
            BackEnd::fatalError(decl.init->loc, "NULL initializer expression");
        }
        //initializer expression is expected to be on the stack
        return decl.csym;
    }
}
#pragma endregion


/***************************************/
#pragma region FloatingPointType

const char* FloatingPointType::name() const
{
    if (name_.empty())
    {
        switch (numBits_)
        {
        case 32: name_ += "float32"; break;
        case 64: name_ += "float64"; break;
        default: throw logic_error("invalid number of bits");
        }
    }
    return name_.c_str();
}


TYPE* FloatingPointType::getConstType()
{
    return this;
}


const char* FloatingPointType::getSuffix() const
{
    switch (numBits_)
    {
    case 32: return "r4";
    case 64: return "r8";
    }
    throw logic_error("invalid number of bits");
}


elem* FloatingPointType::init(IRState& state, VarDeclaration& decl, Expression* exp, elem*)
{
    if (!decl.init)
    {
        assert(exp == NULL);
        return Const<double>::create(*this, .0, state.getBlock(), decl.loc);
    }
    else
    {
        if (!exp)
        {
            BackEnd::fatalError(decl.init->loc, "NULL initializer expression");
        }
        return decl.csym;
    }
}
#pragma endregion


/***************************************/
const char* BooleanType::name() const
{
    return "bool";
}


const char* BooleanType::getSuffix() const
{
    return "u1";
}


const char* BooleanType::getElemSuffix() const
{
    return "i1";
}


TYPE* BooleanType::getConstType()
{
    return TYPE::Int32;
}


elem* BooleanType::init(IRState& state, VarDeclaration& decl, Expression* exp, elem*)
{
    if (!decl.init)
    {
        assert(exp == NULL);
        return Const<unsigned>::create(*this, 0, state.getBlock(), decl.loc);
    }
    else
    {
        if (!exp)
        {
            BackEnd::fatalError(decl.init->loc, "NULL initializer expression");
        }
        return decl.csym;
    }
}


/***************************************/
const char* CharType::name() const
{
    switch (size_)
    {
    case 1: return "unsigned int8";
    case 4: return "unsigned int32";
    default: assert(false);
    case 2: return "char"; // IL Char is unicode16
    }
}


const char* CharType::getElemSuffix() const
{
    switch (size_)
    {
    case 1: return "i1";
    case 4: return "i4";
    default: assert(false);
    case 2: return "i2";
    }
}


const char* CharType::getSuffix() const
{
    switch (size_)
    {
    case 1: return "u1";
    case 4: return "u4";
    default: assert(false);
    case 2: return "u2";
    }
}


TYPE* CharType::getConstType()
{
    return TYPE::Int32;
}


elem* CharType::init(IRState& state, VarDeclaration& decl, Expression* exp, elem*)
{
    if (!decl.init)
    {
        assert(exp == NULL);
        return Const<unsigned>::create(*this, 0, state.getBlock(), decl.loc);
    }
    else
    {
        if (!exp)
        {
            BackEnd::fatalError(decl.init->loc, "NULL initializer expression");
        }
        return decl.csym;
    }
}


// quote names that are not fully qualified by assembly name
static void quoteIfNeeded(string& name)
{
    if (!name.empty() && name[0] != '[')
    {
        name = '\'' + name + '\'';
    }
}


/***************************************/
ObjectType::ObjectType(Type* type) : type_(type)
{
}


const char* ObjectType::name() const
{
    if (name_.empty())
    {
        name_ = "class ";
        name_ += getSuffix();
    }
    return name_.c_str();
}


const char* ObjectType::getSuffix() const
{
    if (suffix_.empty())
    {
        if (type_)
        {
            bool quote = true;
            if (ClassDeclaration* cd = DEREF(type_).isClassHandle())
            {
                if (cd->ident == Id::object || cd->ident == Lexer::idPool("__object"))
                {
                    suffix_ = "[mscorlib]System.Object";
                    quote = false;
                }
                if (cd->isNested())
                {
                    quote = false;
                }
            }
            if (suffix_.empty())
            {
                Temporary<unsigned char> disable(type_->mod, 0);
                suffix_ = type_->toChars();
                if (quote) quoteIfNeeded(suffix_);
            }
        }
        else
        {
            suffix_ = "[mscorlib]System.Object";
        }
    }
    return suffix_.c_str();
}


elem* ObjectType::init(IRState& irstate, VarDeclaration& decl, Expression*, elem*)
{
    if (decl.init)
    {
        return decl.csym;
    }
    if (Symbol* sym = decl.csym)
    {
        if (Variable* v = sym->isVariable())
        {
            if (v->isStatic())
            {
                irstate.add(*new Null);
            }
        }
    }
    return NULL;
}


static wostream& emitCastInstr(wostream& out)
{
#if 0
    if (global.params.safe)
    {
        out << "castclass ";
    }
    else
#endif
    {
        out << "isinst ";
    }
    return out;
}


void ObjectType::emitConversion(wostream& out)
{
    emitCastInstr(out) << getSuffix();
}


ClassDeclaration* ObjectType::getClassDecl()
{
    if (type_ && type_->ty == Tclass)
    {
        return static_cast<TypeClass*>(type_)->sym;
    }
    return NULL;
}


/***************************************/
IdentType::IdentType(Type& type, TYPE* equivType) : equivType_(equivType)
{
    name_ = type.toChars();
}


IdentType::~IdentType()
{
    delete equivType_;
}


elem* IdentType::init(IRState&, VarDeclaration&, Expression*, elem*)
{
    NOT_IMPLEMENTED(NULL);
}


const char* IdentType::name() const
{
    return name_.c_str();
}


void IdentType::emitConversion(wostream& out)
{
    emitCastInstr(out) << name();
}


ArrayType* IdentType::isArrayType()
{
    return equivType_ ? equivType_->isArrayType() : NULL;
}


/***************************************/
PointerType::PointerType(TYPE& pointedType, bool managed)
    : pointedType_(pointedType)
    , unmanagedType_(NULL)
{
    if (pointedType.isPointerType())
    {
        throw runtime_error("pointer to pointer not allowed in IL");
    }
    if (!managed)
    {
        unmanagedType_ = this;
    }
}


PointerType* PointerType::getUnmanagedType()
{
    if (unmanagedType_ == NULL)
    {
        unmanagedType_ = new PointerType(pointedType_, false);
    }
    return unmanagedType_;
}


const char* PointerType::name() const
{
    if (name_.empty())
    {
        name_ = pointedType_.name();
        if (isManaged())
        {
            name_ += "&";
        }
        else
        {
            name_ += "*";
        }
    }
    return name_.c_str();
}


elem* PointerType::init(IRState&, VarDeclaration& decl, Expression*, elem*)
{
    return decl.csym;
}


void PointerType::emitConversion(wostream& out)
{
    IF_DEBUG(out << "// pointer conversion elided");    
}


/***************************************/
void ValueType::emitConversion(wostream& out)
{
    out << "conv." << getSuffix();
}


/***************************************/
const char* StructType::name() const
{
    if (name_.empty())
    {
#if VALUETYPE_STRUCT
        name_ = "valuetype ";
#else
        name_ = "class ";
#endif
        name_ += getSuffix();
    }
    return name_.c_str();
}


elem* StructType::init(IRState& irstate, VarDeclaration& vd, Expression*, elem*)
{
    return BackEnd::construct(vd.loc, *this);
}


const char* StructType::getSuffix() const
{
    if (suffix_.empty())
    {
        if (type_)
        {
            if (Dsymbol* sym = type_->sym)
            {      
                suffix_ = sym->toPrettyChars();
            }
            else
            {
                suffix_ = type_->toChars();
            }
            quoteIfNeeded(suffix_);
        }
        else
        {
    #if VALUETYPE_STRUCT
            suffix_ = "[mscorlib]System.ValueType";
    #else
            suffix_ = "[mscorlib]System.Object";
    #endif
        }
    }
    return suffix_.c_str();
}


const char* StructType::getElemSuffix() const
{
    if (elemSuffix_.empty())
    {
#if VALUETYPE_STRUCT
        elemSuffix_ = "any " + suffix_;
#else
        elemSuffix_ = "ref";
#endif
    }
    return elemSuffix_.c_str();
}


void StructType::emitConversion(wostream&)
{
}


StructDeclaration* StructType::getStructDecl()
{
    if (type_ && type_->ty == Tstruct)
    {
        return static_cast<TypeStruct*>(type_)->sym;
    }
    return NULL;
}


/***************************************/
EnumType::EnumType(TypeEnum* type) : ValueType(), type_(type)
{
}


void EnumType::emitConversion(wostream& out)
{
}


const char* EnumType::name() const
{
    if (name_.empty())
    {
        name_ = "valuetype ";
        name_ += getSuffix();
    }
    return name_.c_str();
}


const char* EnumType::getSuffix() const
{
    if (type_)
    {
        if (suffix_.empty())
        {
            //declared inside of an aggregate?
            if (AggregateDeclaration* ad = DEREF(type_).sym->isMember())
            {
                suffix_ += ad->toPrettyChars();
                suffix_ += '.';
            }
            suffix_ += type_->toChars();
        }
        return suffix_.c_str();
    }
    else
    {
        return "[mscorlib]System.ValueType";
    }
}


/***************************************/
StringType::StringType(Loc loc) : ArrayType(loc, DEREF(Type::twchar))
{
}


const char* StringType::name() const 
{
    return "string";
}


SliceType* StringType::getSliceType(Loc&)
{
    return NULL;
}


elem* StringType::init(IRState& irstate, VarDeclaration& decl, Expression* exp, elem* e)
{
    return irstate.add(*new Null);
}
