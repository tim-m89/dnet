#ifndef INSTRUCTION_H__7B2B56B8_B089_4703_8231_CDCE12CBE4D3
#define INSTRUCTION_H__7B2B56B8_B089_4703_8231_CDCE12CBE4D3
// -*- tab-width: 4; indent-tabs-mode: nil;  -*-
// vim: tabstop=4:softtabstop=4:expandtab:shiftwidth=4
//
// $Id: instruction.h 24625 2009-07-31 01:05:55Z unknown $
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
#include <string>
#include <vector>
#include "backend.net/backend.h"
#include "backend.net/block.h"  // for AutoManaged
#include "backend.net/elem.h"
#include "backend.net/partialresult.h"
#include "backend.net/msil/mnemo.h"

struct Type; //front-end
struct TYPE; //back-end
class ArrayType;
class AssocArrayType;
class BranchInstruction;
class Method;


class Instruction : public AutoManaged<elem>
{
    Loc* loc_;
    Mnemonic mnemo_;

protected:
    Instruction(block&, Mnemonic, Loc* loc);
    virtual ~Instruction();

    elem* emitILAsm(std::wostream&);

public:
    static Instruction* create(block& blk,
                               Mnemonic m,
                               Loc* loc = 0
                               )
    {
        assert(m != IL_ret); // use block::addRet instead
        return new Instruction(blk, m, loc);
    }

    Mnemonic getMnemonic() const { return mnemo_; }
    int32_t getStackDelta() const;

    bool emitLineInfo(std::wostream& out) const
    {
        if (loc_)
        {
            return BackEnd::instance().emitLineInfo(*this, *loc_, out);
        }
        return false;
    }

    virtual bool isBlockEnd() const;

    Instruction* isInstruction() { return this; }
    virtual BranchInstruction* isBranchInstruction() { return NULL; }
};


///<summary>
/// Emit ISINST and UNBOX.ANY operations. ISINST is used over CASTCLASS
/// because in D a failed cast is expected to return a null reference
/// rather than throw an exception.
///</summary>
class Conversion : public Instruction
{
    TYPE& toType_;

protected:
    Conversion(block&, TYPE& toType, Loc* loc);
    Conversion(block&, Mnemonic, TYPE& toType, Loc* loc);
    elem* emitILAsm(std::wostream&);

public:
    static Conversion* create(block& blk, TYPE& toType, Loc* loc = NULL)
    {
        return new Conversion(blk, toType, loc);
    }

    static Conversion* unbox(block& blk, TYPE& toType, Loc* loc = NULL)
    {
        return new Conversion(blk, IL_unbox_any, toType, loc);
    }
};


///<summary>
/// Emits set_Item and get_Item
///</summary>
class ArrayElemAccess : public Instruction
{
    bool byAddr_;

public:
    enum Op { LOAD, STORE };

protected:
    ArrayElemAccess(block&, Op, const TYPE& elemType, Loc* loc, bool);
    elem* emitILAsm(std::wostream&);

public:
    static elem* load(block&, Variable&, Loc* = NULL, bool addr = false);
    static elem* load(block& blk, const TYPE& elemTYPE, bool addr, Loc* loc = NULL)
    {
        return new ArrayElemAccess(blk, LOAD, elemTYPE, loc, addr);
    }
    static elem* store(block& blk, const TYPE& elemTYPE, Loc* loc = NULL)
    {
        return new ArrayElemAccess(blk, STORE, elemTYPE, loc, false);
    }

private:
    Op op_;
    const TYPE& elemTYPE_;
};


///<summary>
/// Emits ldelem.* and stelem.* instructions
///</summary>
class AssocArrayElemAccess : public Instruction
{
public:
    enum Op { LOAD, STORE };

protected:
    AssocArrayElemAccess(block&, Op, TYPE& elemType, TYPE& indexType, Loc* loc);
    elem* emitILAsm(std::wostream&);

public:
    static AssocArrayElemAccess* load(block& blk,
                                      TYPE& elemType,
                                      TYPE& indexType,
                                      Loc* loc = NULL);

    static AssocArrayElemAccess* store(block& blk,
                                       TYPE& elemType,
                                       TYPE& indexType,
                                       Loc* loc = NULL);

private:
    Op op_;
    TYPE& elemType_;
    TYPE& keyType_;
};


class Box : public Instruction
{
    const TYPE& type_;

    Box(block&, const TYPE&, Loc* loc);
    elem* emitILAsm(std::wostream&);

public:
    static Box* create(block& block, const TYPE& type, Loc* loc = NULL)
    {
        return new Box(block, type, loc);
    }
};



/***************************************************************
 * Flow control
 ***************************************************************/
class CallInstruction : public Instruction
{
    Method& callee_;
    Variable* result_;

    CallInstruction(block&, Mnemonic, Method&, Loc* loc);
    elem* emitILAsm(std::wostream&);

    Variable* isVariable();
    virtual int32_t getStackDelta() const;

    bool isStructDtorCall() const;
    virtual Method* isMethodCall() { return &callee_; }

public:
    static Instruction* create(
        block& blk,     // block that will own the instr
        Mnemonic mnemo, // call, callvirt, newobj
        Method& method,
        Loc* loc = NULL // source code locus of call
        )
    {
        return new CallInstruction(blk, mnemo, method, loc);
    }

    static Instruction* create(
        block& blk,     // block that will own the instr
        Method& method,
        Loc* loc = NULL // source code locus of call
        );
};



class BranchInstruction : public Instruction
{
    BranchInstruction(block&, Mnemonic, Loc* loc);
    elem* emitILAsm(std::wostream&);

public:
    static Instruction* create(block& blk, Mnemonic mnemonic, Loc* loc = NULL)
    {
        return new BranchInstruction(blk, mnemonic, loc);
    }

    virtual BranchInstruction* isBranchInstruction() { return this; }
};



///<summary>
/// Helper for creating branching instructions and labels.
/// It emits both the branch inst opcode and the label.
///</summary>
/// todo: if a Branch-ing point becomes unreachable, it should call
/// Label::decRefCount(), so that we can cull unreachable branch
/// destinations as well [cristiv].
class Branch : public AutoManaged<elem>
{
    Label* label_;   // destination label
    bool endsBlock_;
    Instruction* inst_;

    Branch(block& source, block* dest, bool isBlockEnd, Instruction*);
    elem* emitILAsm(std::wostream&);

public:
    static Branch* create(elem*& cond, block& source, block* dest, Loc* loc = NULL);

    ///create an unconditional branch from source to destination
    static Branch* create(block& source, block* dest, Loc* loc = NULL);

    virtual Label* isBranching() { return label_; }
    virtual bool isBlockEnd() const { return endsBlock_; }
    virtual Instruction* isInstruction() { return inst_; }

    void setLabel(Label& label);
};


///<summary>
/// Helper for creating switch staements.
///</summary>
class SwitchInstruction : public Instruction
{	
	typedef std::vector<Label*>  LabelsArray;
	LabelsArray labels_;
	
	SwitchInstruction(block&, const LabelsArray&, Mnemonic, Loc* loc);

public:   
    static Instruction* create(block& block,
                               const LabelsArray& labels,
                               Mnemonic mnemonic,
                               Loc* loc = NULL
                               )
    {
        return new SwitchInstruction(block, labels, mnemonic, loc);
    }

    elem* emitILAsm(std::wostream&);
};


///<summary>
///Pseudo-instruction: XOR the boolean on top of the evaluation stack with 1
///</summary>
class LogicalNegation : public Instruction
{
    elem* emitILAsm(std::wostream&);
    LogicalNegation(block& blk, Loc* loc) : Instruction(blk, IL_xor, loc)
    {
    }
    int32_t getMaxStackDelta() const
    {
        return 1; // because of ldc.i4 1
    }
    bool isLogicalNegation() const
    {
        return true;
    }

public:
    static Instruction* create(block& blk, Loc* loc = 0)
    {
        return new LogicalNegation(blk, loc);
    }
};



///<summary>
///Generate code for calling property getters.
///</summary>
///Stack delta is zero: pops the object off the stack, pushes property.
class PropertyGetter : public elem
{
    TYPE& type_;
    std::string name_;
    std::string propType_;

    PropertyGetter(TYPE& type, const char* name, const char* propType) 
        : type_(type)
        , name_(name)
        , propType_(propType)
    {
    }

    elem* emitILAsm(std::wostream& out);

public:
    static elem* create(TYPE& type, const char* name, const char* propType)
    {
        return new PropertyGetter(type, name, propType);
    }
};


///<summary>
/// Emit code to create new associative array on the evaluation stack.
///</summary>
class NewAssociativeArray : public elem
{
    const AssocArrayType& type_;

    elem* emitILAsm(std::wostream&);
    int32_t getStackDelta() const;

    explicit NewAssociativeArray(const AssocArrayType& type);
    NewAssociativeArray* isNewAssociativeArray() { return this; }

public:
    static NewAssociativeArray* create(const AssocArrayType& type)
    {
        return new NewAssociativeArray(type);
    }
};


///<summary>
///Generate ldftn instruction.
///</summary>
class LoadFunc : public Instruction
{
    Method& func_;

    LoadFunc(block&, Method&, Loc* loc);
    elem* emitILAsm(std::wostream&);
    LoadFunc* isLoadFunc() { return this; }

public:
    static Instruction* create(
        block& blk,     // block that will own the instr
        Method& method,
        Loc* loc = NULL // source code locus of call
        )
    {
        return new LoadFunc(blk, method, loc);
    }
    //hack
    virtual Method* isMethod() { return &func_; }
};


class RetInstruction : public Instruction
{
    const int32_t delta_;

public:
    RetInstruction(block& block, Loc* loc, int32_t delta)
        : Instruction(block, IL_ret, loc)
        , delta_(delta)
    {
        assert(delta == 0 || delta == -1);
    }

    bool isBlockEnd() const { return true; }
    int32_t getStackDelta() const { return delta_; }
};



class LogicalExpTail : public elem, public EmitHooks
{
    Branch* shortCircuit_;
    LogicalExpTail* lhs_;
    elem* rhs_;
    bool optimized_;

#pragma region EmitHooks Interface
    virtual bool preEmit(elem&);
    virtual void postEmit(elem*);

    ///override elem's stack delta
    virtual int32_t stackDelta(const elem&, bool maxDelta);

    virtual bool isReferenced(const elem&) const;
#pragma endregion EmitHooks Interface

    LogicalExpTail* isLogicalExpTail()
    {
        return this;
    }
    elem* emitILAsm(std::wostream&);
    bool canDelete() { return false; }

public:
    LogicalExpTail(Branch* shortcircuit, elem* lhs, elem* rhs);
    Instruction* optimizeBranch(block& source, block* dest, Loc*);
};

#endif // INSTRUCTION_H__7B2B56B8_B089_4703_8231_CDCE12CBE4D3
