#ifndef ELEM_H__813C3650_8148_414F_BDC8_E01204852FF8
#define ELEM_H__813C3650_8148_414F_BDC8_E01204852FF8
// -*- tab-width: 4; indent-tabs-mode: nil;  -*-
// vim: tabstop=4:softtabstop=4:expandtab:shiftwidth=4
//
// $Id: elem.h 24625 2009-07-31 01:05:55Z unknown $
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
#include <climits>
#include <iosfwd>
#include <memory>
#include <stdint.h>
#include "backend.net/indentable.h"
#include "backend.net/utils.h"
#include "lexer.h" // for TOK

class Aggregate;
class ASlice;
class Class;
class Delegate;
class Field;
class Instruction;
class Label;
class LoadFunc;
class LogicalExpTail;
class Method;
class NewAssociativeArray;
class PartialResult;
class Positional;
class Sequence;
class Slot;
class SliceFiller; // emits calls to runtime helper
class StringLiteral;
class Struct;
class Variable;

struct TYPE;

enum Encoding
{
    e_UTF8,
    e_UTF16,
    e_UTF32,
    e_SystemString,
    e_Default = e_UTF8
};



// These structs are mandated by the front-end: block, elem, dt_t, and Symbol.
// They are forward-declared in the FE. The actual definitions are up to the backend.
struct block;
struct dt_t;
struct elem;
struct Symbol;
// For the purposes of the MSIL-generator backend dt_t is not (yet?) used.
// elem is the base class for all code elements, and Symbol is loosely an
// element that corresponds to a front-end Dsymbol (such as a variable or
// function).


///<summary>
///The purpose of these hooks is to provide a "back-patching" mechanism,
///where code elements can be substituted after they have been generated,
///without physically replacing them. The emit hooks may alter the
///emitted code (for optimization purposes, for example). The stack deltas
///may also have to change, to reflect the changes in the emitted code.
///</summary>
struct EmitHooks
{
    virtual ~EmitHooks() = 0;
    virtual bool preEmit(elem&) = 0;
    virtual void postEmit(elem*) = 0;

    ///override elem's stack delta
    virtual int32_t stackDelta(const elem&, bool maxDelta) = 0;

    virtual bool isReferenced(const elem&) const = 0;
};


///<summary>
/// An element of IL code.
///</summary>
struct elem : Indentable
{
    static const int32_t UNKNOWN_STACK_DELTA = INT_MIN;

    elem();
    virtual ~elem();

    virtual dt_t* isDt() { return NULL; }

    virtual Aggregate* isAggregate() { return NULL; }
    virtual ASlice* isASlice() { return NULL; }
    virtual block* isBlock() { return NULL; }
    virtual Delegate* isDelegate() { return NULL; }
    virtual Field* isField() { return NULL; }
    virtual Instruction* isInstruction() { return NULL; }
    virtual Label* isBranching() { return NULL; }
    virtual LogicalExpTail* isLogicalExpTail() { return NULL; }

    // for optimizing consecutive LD instructions (by replacing with a DUP);
    // returns the loaded var
    virtual Variable* isLoad() { return NULL; }
    virtual LoadFunc* isLoadFunc() { return NULL; }
    virtual Method* isMethod() { return NULL; }
    virtual Method* isMethodCall() { return NULL; }
    virtual NewAssociativeArray* isNewAssociativeArray() { return NULL; }
    virtual PartialResult* isPartialResult() { return NULL; }
    virtual Positional* isPositional() { return NULL; }
    virtual Sequence* isSequence() { return NULL; }
    virtual SliceFiller* isSliceFiller() { return NULL; }
    virtual StringLiteral* isStringLiteral() { return NULL; }
    virtual Slot* isSlot() { return NULL; }
    virtual Symbol* isSymbol() { return NULL; }
    virtual Variable* isStore() { return NULL; }
    virtual Variable* isVariable() { return NULL; }

    virtual bool isConst() const { return false; }
    virtual bool isLogicalNegation() const { return false; }
    virtual bool isNull() const { return false; }

    ///<summary>
    /// emit IL asm code to stream by calling emitILAsm (template method pattern)
    /// call pre and post hooks if installed.
    ///</summary>
    elem* emitIL(std::wostream&);

    int32_t stackDelta() const;
    int32_t maxStackDelta() const;

    virtual void setEmitHooks(EmitHooks*);

    ///<summary>
    /// This method should return true for instructions that cause the
    /// control flow to leave the current block (branch, ret, etc.)
    ///</summary>
    virtual bool isBlockEnd() const { return false; }

    inline block* getOwner() const { return owner_; }
    void setOwner(block* owner) { owner_ = owner; }
    void setOwner(block* owner, Temporary<block*>& tmp) 
    {
        tmp = owner_;
        owner_ = owner;
    }

    //Indentation depth of this elem in the generated IL asm code.
    unsigned depth() const;

    virtual bool canDelete() { return true; }

    //get the number of outstanding elem instances
    static int64_t getCount() { return count_; }

    virtual int32_t getStackDelta() const { return 0; }
    virtual int32_t getMaxStackDelta() const { return getStackDelta(); }

protected:
    // non-copyable by client code, okay for derived
    elem(const elem&) : owner_(NULL), hooks_(NULL)
    { 
        ++count_; 
    }
    
    elem& operator=(const elem&); // non-assignable
    
    virtual elem* emitILAsm(std::wostream&) = 0;

    static int64_t count_; // count outstanding elements
    block* owner_;
    EmitHooks* hooks_;
};


///<summary>
/// Represents statically initialized data
///</summary>
struct dt_t : public elem
{
private:
    elem* elem_;
    const char* dataLabel_;
    TOK tok_;

    virtual elem* emitILAsm(std::wostream&);

public:
    explicit dt_t(elem* e, TOK tok = TOKreserved, const char* dataLabel = NULL)
        : elem_(e)
        , dataLabel_(dataLabel)
        , tok_(tok)
    { }

    virtual dt_t* isDt() { return this; }

    elem* getElem() { return elem_; }
    const char* getDataLabel() const { return dataLabel_; }    
    TOK op() const { return tok_; }
};


///<summary>
///back-end symbol, loosely an corresponds to a front-end Dsymbol
///such as a variable or function
///</summary>
struct Symbol : elem
{
    Symbol() : refCount_(0) { }

    virtual Symbol* isSymbol() { return this; }
    virtual Class*  isClass()  { return NULL; }
    virtual Struct* isStruct() { return NULL; }

    //for binding aggregate members to their "owner" object
    virtual void bind(Variable*, bool = true) { }
    virtual void unbind(bool = false) { }
    virtual bool isBound() const { return false; }
    virtual Variable* getParent() const { return NULL; }

    virtual bool isTypeInfo() const { return false; }

    virtual long incRefCount()
    {
        return ++refCount_;
    }
    virtual long decRefCount()
    {
        assert(refCount_);
        return --refCount_;
    }
    virtual long refCount() const
    {
        return refCount_;
    }

protected:
    long refCount_;
};


class Sequence : public elem
{
public:
    virtual size_t length() const = 0;
    virtual TYPE* elemTYPE() const = 0;
    virtual Sequence* isSequence() { return this; }
};


///<summary>
/// Shares the emitILAsm logic of another elem
///</summary>
class SharedElem : public elem
{
    elem& shared_;

    elem* emitILAsm(std::wostream& out)
    {
        Temporary<block*> tmp;
        //use the owner's indentation depth
        shared_.setOwner(getOwner(), tmp);
        return shared_.emitIL(out);
    }
    int32_t getStackDelta() const
    {
        return shared_.getStackDelta();
    }
    int32_t getMaxStackDelta() const
    {
        return shared_.getMaxStackDelta();
    }

public:
    explicit SharedElem(elem& shared) : shared_(shared)
    {
    }
};
#endif // ELEM_H__813C3650_8148_414F_BDC8_E01204852FF8
