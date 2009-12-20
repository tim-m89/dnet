#ifndef VARIABLE_H__7AFCCDB0_7C54_4527_9538_2672C0C8D43C
#define VARIABLE_H__7AFCCDB0_7C54_4527_9538_2672C0C8D43C
// -*- tab-width: 4; indent-tabs-mode: nil;  -*-
// vim: tabstop=4:softtabstop=4:expandtab:shiftwidth=4
//
// $Id: variable.h 24625 2009-07-31 01:05:55Z unknown $
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
#if VALUETYPE_STRUCT
 #include "declaration.h" // for VarDeclaration
 #include "id.h"    // for Id::This
 #include "mtype.h" // for Tstruct
#endif
#include "backend.net/block.h"
#include "backend.net/elem.h"

#pragma region Forward decls.
//front-end
struct Loc;
struct Declaration;
struct StructDeclaration;
struct Type;
//back-end
class AssocArrayType;
class SliceType;
#pragma endregion


//The .NET backend has no use for comdat sections, repurpose storage class
#define STCpinned STCcomdat


class Variable : public Symbol
{
public:
    explicit Variable(VarDeclaration& decl, Type* = NULL);

    virtual Variable* isVariable() { return this; }
    virtual bool isStatic() const;
    virtual bool isNested() const { return false; }

    ///reference this variable for reading; typically 
    ///the implementation loads a field or argument onto
    ///the evaluation stack
    virtual elem* load(block&, Loc* = NULL, bool addr = false) = 0;

    ///reference variable for writing; typically this
    ///translates into some kind of STORE instruction 
    ///(starg, stfld, stelem.*, etc.)
    virtual elem* store(block&, Loc* = NULL) = 0;

    virtual Method* isMethod();

    VarDeclaration& getDecl() { return decl_; }
    Type* getType();
    void setType(Type* t, Loc);

    virtual void releaseSlot() { }  // for managing nested locals
    virtual const char* getName();

    Positional* isLocalOrParam() { return isPositional(); }
    StructDeclaration* isStructMember();

    //return true if this variable is referenced from a closure
    bool hasClosureReference();

    //return true if type is Treference and the storage_class is NOT ref
    bool isRefType();
    
    void setName(Identifier* name)
    {
        name_ = name;
    }

protected:
    VarDeclaration& decl_;
    Type* type_;
    Identifier* name_;
};


///<summary>
/// Helper class for loading other elem-s via the Variable interface
///</summary>
class VarAdapter : public Variable
{
    elem& elem_;

    virtual elem* emitILAsm(std::wostream&);

    virtual elem* load(block&, Loc* = NULL, bool addr = false);
    virtual elem* store(block&, Loc* = NULL);
    virtual bool isConst() const
    {
        return elem_.isConst();
    }

public:
    explicit VarAdapter(VarDeclaration& decl, elem&);
};


template<typename T>
class BaseField : public Variable
{
protected:
    BaseField(VarDeclaration& vdecl, Type* type = NULL)
        : Variable(vdecl, type)
    { }

public:
    void bind(Variable* parent, bool bound = true)
    {
        assert(parent || global.errors);
        assert(!static_cast<T*>(this)->isBound() || !bound);
        static_cast<T*>(this)->setParent(parent);
        static_cast<T*>(this)->setBound(bound);
    }
    void unbind(bool resetParent = false)
    {
        static_cast<T*>(this)->setBound(false);
        if (resetParent)
        {
            static_cast<T*>(this)->setParent(NULL);
        }
    }
};


class BaseArrayElem : public AutoManaged<BaseField<BaseArrayElem> >
{
    Variable* parent_;
    bool bound_;

protected:
    BaseArrayElem(block& blk, VarDeclaration& vdecl, Type* type = NULL)
        : AutoManaged<BaseField<BaseArrayElem> >(blk, vdecl, type)
        , parent_(NULL)
        , bound_(false)
    {
    }

public:
    bool isBound() const { return bound_; }
    void setBound(bool bound) { bound_ = bound; }
    Variable* getParent() const { return parent_; }
    void setParent(Variable* parent) { parent_ = parent; }
};


class ArrayElem : public BaseArrayElem
{
    ArrayElem(block&, VarDeclaration& decl, Type*);
    elem* emitILAsm(std::wostream&);

public:
    static ArrayElem* create(block& blk, VarDeclaration& decl, Type* type)
    {
        return new ArrayElem(blk, decl, type);
    }
    virtual elem* load(block&, Loc*, bool);
    virtual elem* store(block&, Loc*);
};


class AssocArrayElem : public BaseArrayElem
{
    Variable* keyVar_;

    AssocArrayElem(block&, Variable& arrayVar, Variable* keyVar, Type& type);

    elem* emitILAsm(std::wostream&);
    AssocArrayType& getAssocArrayType(Loc*);

public:
    static AssocArrayElem* create(block& blk, Variable& arrayVar, Variable* keyVar, Type& type)
    {
        return new AssocArrayElem(blk, arrayVar, keyVar, type);
    }

    virtual elem* load(block&, Loc*, bool);
    virtual elem* store(block&, Loc*);

    ///<summary>
    /// If the type of the key is a D string (represented internally as an array of bytes),
    /// convert it to System.String (System.String's Equals implements lexicographical
    /// string comparison, while System.Array's Equals only compares object references).
    ///</summary>

    static bool convertKey(block&, AssocArrayType&, Variable* key, bool swap = false);
};


///<summary>
///Generate code to load a variable on the eval stack, taking
///care of the possibility of the var being an array (or slice)
///</summary>
SliceType* loadArray(block&, Variable&, bool inSliceExp = false);


/*******************************************************************/
template<typename T>
inline void loadRefParent(T& f, block& blk, Loc* loc)
{
    if (!f.isBound())
    {
        if (Variable* parent = f.getParent())
        {
#if VALUETYPE_STRUCT
            bool loadAddr =
                (DEREF(parent->getType()).ty == Tstruct) &&
                (parent->getDecl().ident != Id::This);
#else
            static const bool loadAddr = false;
#endif
            parent->load(*blk.addNewBlock(loc), loc, loadAddr);
        }
    }
    f.unbind();
}


#endif // VARIABLE_H__7AFCCDB0_7C54_4527_9538_2672C0C8D43C
