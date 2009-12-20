#ifndef MEMBER_H__27B359F0_34FE_4295_93FB_CA03539885E9
#define MEMBER_H__27B359F0_34FE_4295_93FB_CA03539885E9
// -*- tab-width: 4; indent-tabs-mode: nil;  -*-
// vim: tabstop=4:softtabstop=4:expandtab:shiftwidth=4
//
// $Id: member.h 24625 2009-07-31 01:05:55Z unknown $
//
// Method, Field
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

#include <string>
#include "aggregate.h"
#include "id.h"
#include "backend.net/array.h"
#include "backend.net/block.h"
#include "backend.net/elem.h"
#include "backend.net/irstate.h"
#include "backend.net/type.h"
#include "backend.net/msil/variable.h"

/****************************/
#pragma region Forward Decls

struct Dsymbols;
struct Declaration;
struct FuncDeclaration;
struct Identifier;
struct Initializer;
struct TYPE;      // backend
struct VarDeclaration;
#pragma endregion
/****************************/


class Member : public Symbol
{
    Declaration& decl_;

protected:
    Member(Declaration&);

public:
    inline Declaration& getDecl() const { return decl_; }
    AggregateDeclaration* getAggregateDecl() { return decl_.isMember(); }

    //return the class that owns this member
    Class* getClass();

    //return the aggrege that owns this member (may be a class or a struct)
    Aggregate* getAggregate();
};



///<summary>
/// Module or class field.
///</summary>
class Field : public BaseField<Field>
{
    //ctor is private to prevent further inheritance
    //so that cloning this Field (see isSymbol) does not slice it
    Field(VarDeclaration&, bool isStatic);

    elem* emitILAsm(std::wostream&);

public:
    static Field* create(VarDeclaration& decl, bool isStatic)
    {
        return new Field(decl, isStatic);
    }

    virtual Field* isField() { return this; }

    virtual elem* load(block&, Loc* = NULL, bool = false);
    virtual elem* store(block&, Loc* = NULL);

    virtual Symbol* isSymbol();

    const TYPE& getTYPE() const;

    bool isStatic() const { return static_; }

    unsigned* position() const { return NULL; }

    Variable* getParent() const
    {
        return parent_;
    }
    void setParent(Variable* parent)
    {
        parent_ = parent;
    }

    bool isBound() const { return bound_; }
    void setBound(bool bound) { bound_ = bound; }

    bool isTypeInfo() const { return typeinfo_; }
    void setTypeInfo() { typeinfo_ = true; }

private:
    Variable* parent_;  // parent instance, if member of an aggregate
    dt_t* dt_;          // for TLS

    bool static_ : 1;
    bool silent_ : 1;
    bool bound_ : 1;    // bound to parent?
    bool typeinfo_ : 1;
};



///<summary>
/// Map variables in the parent activation record to fields in an object.
///</summary>
/// Nested function calls need to "see" the local variables in the
/// scope of the parent's activation record. This is achieved by making
/// the nested call a method of a context object class.
/// NOTE: nested functions will not be able to see variables of pointer
/// type in the parent scope.
class Closure
{
    typedef std::map<Variable*, Field*> VarMap;
    VarMap varMap_;
    Variable* contextObj_;

public:
    Closure() : contextObj_(NULL)
    {
    }

    Field* isMapped(Variable&);
    bool addMapping(Variable& v, Field* f)
    {
        return varMap_.insert(std::make_pair(&v, f)).second;
    }
    /// generate code that copies variables to fields
    void copyIn(block&, Variable* contextObj);
};


//The front-end synthesizes special method that correspond to
//array and string properties, etc.
enum SynthProp
{
    prop_None,
    prop_Dup,
    prop_Reverse,
    prop_Sort,
    prop_Len,
    prop_Keys,
    prop_Values
};


///<summary>
/// For generating global functions and class methods
/// (they all translate to a .method in IL)
///</summary>
class Method : public Member
{
public:
    Method(FuncDeclaration&, unsigned depth, bool nested = false, Dsymbol* = NULL);
    ~Method();

    Method* isMethod() { return this; }

    /// Return the instance on which this method is called, or null
    Variable* getObject() { return obj_; }

    void emitILProto(std::wostream&, bool full = false);
    void emitRetType(std::wostream&);

    const std::wstring& getProto(bool recompute = false);

    /// Programmatically set the prototype of this method.
    void setProto(const std::wstring& proto, int32_t argCount);

    /// Add a local variable to the body of this method / function
    Variable* addLocal(VarDeclaration&, Type* = NULL);

    //for declaring locals in nested scopes
    void setVarScope(block* block) { varScope_ = block; }
    block* getVarScope() const { return varScope_; }

    /// Get the stack delta for calling this method
    int32_t getCallStackDelta() const;
    
    Type* getRetType() const { return retType_; }

    /// Force return type, use this for compiler-generated,
    /// special methods only!
    void setRetType(Type* type, bool needsConversion = true)
    {
        retType_ = type;
        needsConversion_ = needsConversion;
    }
    bool needsConversion() const { return needsConversion_; }

    /// Determines the best way to call this method:
    /// call or callvirt
    Mnemonic getCallInstr() const;

    /// Return the position where variable args begin,
    /// or -1 if does not have var args.
    int32_t vaOffs() const
    {
        return vaOffs_;
    }

    void generateBody();
    block& getBody() { return body_; }

    //may be different from args_.size() when overriden by setProto
    int32_t getArgCount() const { return argCount_; }

    LocalsBlock& getLocals() { return locals_; }
    block& getArgs() { return args_; }

    CtorDeclaration* isCtor() { return getDecl().isCtorDeclaration(); }
    FuncDeclaration& funcDecl() const;

    Closure* getClosure(bool create = true);
    void setClosure(Closure* context);

    SynthProp isSynthProp() const { return synthProp_; }
    void setSynthProp(SynthProp synthProp, bool aaProp = false)
    {
        synthProp_ = synthProp;
        aaProp_ = aaProp;
    }
    bool isAssocArrayProp() const { return aaProp_; }

    void setTemplate() { template_ = true; }
    bool isTemplate() const { return template_; }

    bool isNested() const { return nested_; }
    bool isNestedMember() const { return nestedMember_; }

    void setNestedMember(bool member)
    {
        assert(nested_);
        nestedMember_ = member;
    }

    Variable* getResult() const { return ret_; }

    bool isDelegatedTo() const { return delegatedTo_; }
    void setDelegatedTo() { delegatedTo_ = true; }

    bool convertArrays() const;

    bool isOptimizedOut() const { return optOut_; }
    void optimizeOut(bool optOut = true) { optOut_ = optOut; }

    bool returnsRef() const;
    bool isSetProto() const { return setProto_; }

    Method* parent() const;

private:
    elem* emitILAsm(std::wostream&);
    void emitILArgs(std::wostream&);
    void emitILBody(std::wostream&);

    ///Make sure that a method that overrides a base class method does
    ///not lower its visibility
    void checkVisibility(FuncDeclaration&);

    bool returnsRef(FuncDeclaration&) const;
    virtual void bind(Variable* v, bool = true) 
    {
        obj_ = v;
    }

private:
    Type*           retType_;
    std::wstring    proto_;
    block           args_;
    block           body_;
    LocalsBlock     locals_;
    Variable*       ret_;
    Variable*       obj_;
    int32_t         argCount_;
    int32_t         vaOffs_;        // -1 if no var args
    block*          varScope_;
    Closure*        closure_;
    bool            setProto_ : 1;  // prototype set programmatically?
    bool            needsConversion_ : 1;
    bool            template_ : 1;
    const bool      nested_ : 1;
    bool            nestedMember_ : 1;
    bool            delegatedTo_ : 1; // for delegating constructors
    bool            optOut_ : 1;
    bool            aaProp_ : 1;    //assoc array prop?
    SynthProp       synthProp_ : 8;
    Dsymbol*        parent_;
};


///<summary>
/// Code element that loads "this" on the evaluation stack.
///</summary>
///<note>
/// This element uses less memory than the result of makeThisArg
///</note>
class ThisArg : public elem
{
    elem* emitILAsm(std::wostream& out)
    {
        indent(out) << "ldarg.0\n";
        return this; 
    }

    int getStackDelta() const { return 1; }
}; // ThisArg


template<typename T>
inline const char* ctorProto(const T&)
{
    return "::.ctor()";
}


inline const char* ctorProto(ClassDeclaration& decl)
{
    if (decl.ident == Id::TypeInfo)
    {
        return "::.ctor(class [mscorlib]System.Type)";
    }
    return "::.ctor()";
}


///<summary>
/// Helper for emitting a call to the default .ctor (supplied by the compiler)
/// from either newobj or a derived class ctor, when no ctor was provided in the D source.
///</summary>
template<typename T>
class DefaultCtorCall : public AutoManaged<elem>
{
    T& decl_;
    Mnemonic op_;

public:
    DefaultCtorCall(block& blk, Mnemonic op, T& decl)
        : AutoManaged<elem>(blk)
        , decl_(decl), op_(op)
    {
        assert(op == IL_call || op == IL_newobj);
    }
    int32_t getStackDelta() const
    {
        return op_ == IL_newobj ? 1 : 0;
    }
    elem* emitILAsm(std::wostream& out)
    {
        indent(out) << op_ << " instance void " << className(decl_) << ctorProto(decl_) << '\n';
        return this;
    }
};


//return true if it has thread local storage, or if it contains a TLS field
bool hasTLS(Declaration& decl);

#endif // MEMBER_H__27B359F0_34FE_4295_93FB_CA03539885E9
