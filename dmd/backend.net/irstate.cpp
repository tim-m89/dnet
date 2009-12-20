// -*- tab-width: 4; indent-tabs-mode: nil;  -*-
// vim: tabstop=4:softtabstop=4:expandtab:shiftwidth=4
//
// $Id: irstate.cpp 24625 2009-07-31 01:05:55Z unknown $
//
// Copyright (c) 2008, 2009 Cristian L. Vlasceanu
// Copyright (c) 2008, 2009 Ionut-Gabriel Burete

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
#include <cassert>
#include <sstream>
#include <stdexcept>
#include <vector>
#include <map>
#include <algorithm>
#include "aggregate.h"
#include "declaration.h"
#include "enum.h"
#include "id.h"
#include "init.h"
#include "statement.h"
#include "backend.net/hiddenmethod.h"
#include "backend.net/irstate.h"
#include "backend.net/irstateutil.h"
#include "backend.net/type.h"
#include "backend.net/msil/assembly.h"
#include "backend.net/msil/assocarrayutil.h"
#include "backend.net/msil/arraylen.h"
#include "backend.net/msil/classemit.h"
#include "backend.net/msil/const.h"
#include "backend.net/msil/deref.h"
#include "backend.net/msil/except.h"
#include "backend.net/msil/label.h"
#include "backend.net/msil/member.h"
#include "backend.net/msil/null.h"
#include "backend.net/msil/slice.h"
#include "backend.net/msil/structemit.h"
#include "backend.net/msil/types.h" //for PointerType
#include "backend.net/msil/varargs.h"

using namespace std;


static FuncDeclaration* isFuncMember(Dsymbol& dsym)
{
    Dsymbol* parent = dsym.toParent();
    return parent ? parent->isFuncDeclaration() : NULL;
}


///<summary
///Verify that non-ref foreach arguments are not modified from within the loop;
///do the same for the foreach aggregate.
/// Note that there may be cases that escape this check, for example modifications
/// via aliases -- todo: explore detecting such cases as well
///</summary>
static void verifyForeachRef(Loc& loc, VarDeclaration& decl, Expression* aggr)
{
#if 0
    //if loc is not given, it is a synthesized expression -> do not issue a warning
    if (loc.filename && loc.linnum && !decl.isRef())
    {
        if (aggr)
        {
            auto_ptr<block> blk(new block(NULL, loc, 0));
            IRState temp(*blk);
            if (decl.csym == aggr->toElem(&temp))
            {
                WARNING(loc, "aggregate is modified inside foreach loop");
                return;
            }
        }
        bool nonRefAssign = false;
        if (decl.storage_class & STCforeach)
        {
            nonRefAssign = true;
        }
        else if (FuncDeclaration* fd = isFuncMember(decl))
        {
            if (fd->fes)
            {
                nonRefAssign = true;
            }
        }
        if (nonRefAssign)
        {
            WARNING(loc, "assignment to non-reference");
        }
    }
#endif
}



//Create nested local variables block
auto_ptr<block> makeNestedLocalsBlock(IRState& irState, Loc& loc, bool newBlock)
{
    block* scopeBlock = &irState.getBlock();
    if (newBlock)
    {
        scopeBlock = irState.getBlock().addNewBlock(&loc);
        scopeBlock->incDepth();
        scopeBlock->setUseBrackets(true);
        irState.setBlock(*scopeBlock);
    }
    return auto_ptr<block>(new LocalsBlock(scopeBlock, loc, scopeBlock->depth()));
}



//Wrapper for Expression::toElem()
elem& exprToElem(IRState& irstate, Expression& exp)
{
    elem* e = exp.toElem(&irstate);
    if (!e)
    {
        BackEnd::fatalError(exp.loc, exp.toChars());
    }
    assert(e->getOwner());
    return *e;
}


elem& loadExp(IRState& irState, Expression& exp, Variable& v, bool loadAddr)
{
    if (!loadAddr && exp.type)
    {
        loadAddr = (exp.type->ty == Tpointer) && (exp.type != v.getType());
    }
    return *v.load(irState.getBlock(), &exp.loc, loadAddr);
}


elem& loadExp(IRState& irState,
              Expression& exp,
              Variable** var,
              bool loadAddr,
              bool inAssignment
              )
{
    elem& e = exprToElem(irState, exp);
    if (Variable* v = e.isVariable())
    {
        if (inAssignment)
        {
            VarDeclaration& vdecl = v->getDecl();
            verifyForeachRef(exp.loc, vdecl, irState.foreachAggr());

            //ensure the stack layout expected by a "store indirect" instr.
            if (vdecl.isRef())
            {
                Temporary<unsigned> storage(vdecl.storage_class);
                vdecl.storage_class &= ~STCref;
                v->load(irState.getBlock(), &exp.loc);
            }
        }
#if VALUETYPE_STRUCT
        if (loadAddr && (v->getDecl().ident == Id::This))
        {
            loadAddr = false;
        }
#endif
        if (Variable* r = loadExp(irState, exp, *v, loadAddr).isVariable())
        {
            v = r;
        }
        if (var)
        {
            *var = v;
        }
    }
    return e;
}


static elem& loadAsBool(IRState& irState, Expression& exp)
{
    elem* result = &exprToElem(irState, exp);
    block& blk = irState.getBlock();
    if (Variable* v = result->isVariable())
    {
        result = v->load(blk, &exp.loc);
    }
    if (DEREF(exp.type).ty == Tclass)
    {
        irState.add(*new Null);
        result = Instruction::create(blk, IL_cgt_un);
    }
    else if (DEREF(exp.type).ty != Tbool)
    {
        Const<int>::create(*TYPE::Int32, 0, blk, exp.loc);
        result = Instruction::create(blk, IL_cgt_un);
    }
    return *result;
}


///<summary>
/// Insert initializer code, making sure that it goes to the proper location.
/// For example, static constants need to be initialized inside static constructors.
/// Called for IntegerExp, RealExp, NullExp and array and string literals.
///</summary>
static elem* init(IRState& irstate, Expression& exp, elem* e)
{
    //are we in the context of a variable declaration?
    if (VarDeclaration* decl = irstate.hasVarDecl())
    {
        if (decl->init) // does have initializer?
        {
            EmitInBlock saveCurrentBlock(irstate);
            TYPE& type = toILType(exp.loc, decl->type);
            return irstate.addInit(type.initVar(irstate, *decl, &exp, e));
        }
    }
    return e;
}


///<summary>
/// Generate code for the given expression
/// inside one block so that it can be handled atomically, as one element
/// by functions such as block::swapTail
///</summary>
static elem* evalExpBlock(IRState& irstate, Expression& exp)
{
    block* blk = irstate.getBlock().addNewBlock(exp.loc);
    EmitInBlock group(irstate, *blk);
    return &loadExp(irstate, exp);
}


Method& toMethod(Dsymbol& sym)
{
    Method* method = NULL;
    if (sym.csym)
    {
        method = sym.csym->isMethod();
    }
    if (!method)
    {
        BackEnd::fatalError(sym.loc, "function expected");
    }
    return *method;
}


void IRBase::pop()
{
    assert(!stack_.empty());
    block_ = stack_.back();
    stack_.pop_back();
}


IRState::IRState(block& blk)
    : IRBase(blk)
    , decl_(NULL)
    , break_(NULL)
    , cont_(NULL)
    , default_(NULL)
    , ret_(NULL)
    , array_(NULL)
    , encoding_(e_UTF8)
    , inCall_(NULL)
    , foreachAggr_(NULL)
{
    ModuleEmitter::getCurrent()->push(*this);
}


IRState::~IRState()
{
    ModuleEmitter::getCurrent()->pop(*this);
}


void IRState::emitInBlock(block& block, Statement& stat)
{
    EmitInBlock(*this, stat, block);
}


Variable* IRState::genBinaryArgs(BinExp& exp, bool assign, Variable** prev)
{
    Variable* lval = NULL;
    loadExp(*this, DEREF(exp.e1), &lval, false, assign);
    if (lval && prev)
    {
        //create a variable and store the value computed for e1;
        //useful for implementing post-increment and post-decrement
        VarDeclaration* vd = new VarDeclaration(lval->getDecl());
        vd->storage_class &= ~STCref;
        *prev = savePartialResult(*this, *vd, NULL, false /* don't optimize */);
        lval->load(getBlock(), &exp.loc);
    }
    loadExp(*this, DEREF(exp.e2));
    return lval;
}


elem* IRState::createBinaryOp(BinExp& exp,
                              Mnemonic op,
                              Variable** lvalue,
                              bool isAssign,
                              Variable** prev
                              )
{
    //group generated elems (that correspond to the AST children)
    //together, to be treated atomically by block::swapTail()
    EmitInBlock group(*this, *getBlock().addNewBlock(exp.loc));

    Variable* v = genBinaryArgs(exp, isAssign, prev);
    if (lvalue)
    {
        *lvalue = v;
    }
    assert(DEREF(exp.e1).type);
    if (DEREF(exp.e1).type->isString())
    {
#if 0 // the native compiler does not support this, so neither should we
        if (op != IL_add)
        {
            error(exp.loc, "unsupported string operation");
        }
#else
        error(exp.loc, "unsupported string operation");
#endif
    }

    if (v)
    {
        // ECMA 335, 8.9.1 Array Types
        // "Array elements shall be laid out within the array object in row-major order 
        // (i.e., the elements associated with the rightmost array dimension shall be laid 
        // out contiguously from lowest to highest index). The actual storage allocated for 
        // each array element can include platform-specific padding. (The size of this storage, 
        // in bytes, is returned by the sizeof instruction when it is applied to the type of 
        // that array's elements.)"
        //
        // From the above I infer that as long as the front-end's view of sizeof() matches IL's
        // managed pointer arithmetic should be fine (TODO: how do I enforce that?) even though
        // it does not get PEVERIFY's seal of approval.
        if (DEREF(exp.e1).type->ty == Tpointer)
        {
            g_havePointerArithmetic = true;
        }
    }
    return Instruction::create(getBlock(), op, &exp.loc);
}



elem* IRState::createAssignOp(BinExp& exp, Mnemonic op, Variable** prev)
{
    Variable* lval = NULL;
    elem* e = createBinaryOp(exp, op, &lval, true, prev);
    if (lval)
    {
        lval->store(getBlock(), &exp.loc);
        return lval;
    }
    else
    {
        error(exp.loc, "assignment to non-lvalue");
    }
    return e;
}


elem* IRState::createLogicalOp(BinExp& exp, Mnemonic op)
{
    block* done1 = new block(&getBlock(), exp.loc, getBlock().depth());
    block* done2 = new block(&getBlock(), exp.loc, getBlock().depth());

    LogicalExpTail* tail = NULL;
    {
        EmitInBlock group(*this, *getBlock().addNewBlock(exp.loc));
        elem* lhs = &loadAsBool(*this, DEREF(exp.e1));
        if ((op == IL_and) && !lhs->isLogicalNegation())
        {
            lhs = LogicalNegation::create(getBlock(), &exp.loc);
            Const<bool>::create(*TYPE::Uint, 0, *done1, exp.loc);
        }
        else
        {
            Const<bool>::create(*TYPE::Uint, 1, *done1, exp.loc);
        }
        // conditional branch, short-circuit the logical expression
        Branch* shortcircuit = Branch::create(lhs, getBlock(), done1);
        elem* rhs = NULL;
        {
            EmitInBlock group(*this, *getBlock().addNewBlock(exp.loc));
            rhs = &loadAsBool(*this, DEREF(exp.e2));
        }
        Branch* br = Branch::create(getBlock(), done2);
        tail = new LogicalExpTail(shortcircuit, lhs, rhs);
        done1->setEmitHooks(tail);
        br->setEmitHooks(tail);
    }

    add(*done1);
    add(*done2);
    return add(*tail);
}


// Handle foreach over associative arrays. Consider this code:
// int [string] a;
// ...
// foreach(k, v; a) ...
// string in D translates to unsigned int8[] in IL, but we are not generating
// Dictionary<unsigned int8[], int32>; rather we're going to generate a dictionary
// where the keys have the [mscorlib]System.String type. We do this to take
// advantage of [mscorlib]System.String::Equals doing "the right thing" --
// a lexicographical comparison ([mscorlib]System.Array::Equals compares just the
// array object references).
//
// When a foreach iteration loop has to be generated for such an associative array
// (implemented in the .NET backend as a Dictionary), the involved delegates will
// see the 'k' Key above as having the type unsigned int8[].
//
// This function takes care of "fixing" the data type.
static void reconcileForeachKeyTypes(VarDeclaration& decl)
{
    if (FuncDeclaration* fd = isFuncMember(decl))
    {
        if (fd->fes)
        {
            ArrayAdapter<Argument*> args(fd->fes->arguments);
            if (!args.empty() && decl.ident->compare(args[0]->ident) == 0)
            {
                decl.type = args[0]->type;
            }
        }
    }
}


Variable* IRState::createVar(VarDeclaration& decl)
{
    assert(decl.csym == NULL);
    Variable* var = NULL;

    reconcileForeachKeyTypes(decl);

    if (hasFuncDecl()) // at function scope? local variable
    {
        Method& method = static_cast<Method&>(DEREF(decl_->csym));
        var = method.addLocal(decl);
        decl.csym = var;
    }
    else if (AggregateDeclaration* aggrDecl = hasAggregateDecl())
    {   // class or some other aggregate type
        if (decl.ident == Id::This)
        {
            return NULL; // skip `this' declaration
        }

        if (toILType(decl).isPointerType())
        {
            error(decl.loc, "pointer fields are not allowed in %s", BackEnd::name());
        }
        block& blk = getBlock();
        Field* f = Field::create(decl, decl.isStatic());
        var = f;
        blk.add(*var);
        decl.csym = var;
        if (!f->isStatic())
        {
            if (Struct* s = DEREF(aggrDecl->csym).isStruct())
            {
                s->addField(f, decl.offset);
            }
        }
    }
    return var;
}


Variable* IRState::createTempVar(VarDeclaration& decl, Type* type)
{
    VarDeclaration* vdecl = new VarDeclaration(decl);
    vdecl->ident = Identifier::generateId("$TMP_");

    if (FuncDeclaration* fdecl = hasFuncDecl())
    {
        return getMethod(decl.loc, *fdecl).addLocal(*vdecl, type);
    }
    else
    {
        return BackEnd::getCurrentMethod(decl.loc).addLocal(*vdecl, type);
    }
}


void IRState::setDecl(AggregateDeclaration* decl)
{
    assert(!decl_);
    decl_ = decl;
}


void IRState::setDecl(EnumDeclaration* decl) 
{
    assert(!decl_);
    decl_ = decl;
}


void IRState::setDecl(FuncDeclaration* decl)
{
    assert(!decl_);
    decl_ = decl;
}


void IRState::setDecl(VarDeclaration* decl)
{
    assert(!decl_);
    decl_ = decl;
}


AggregateDeclaration* IRState::hasAggregateDecl()
{
    return decl_ ? decl_->isAggregateDeclaration() : NULL;
}


ClassDeclaration* IRState::hasClassDecl()
{
    return decl_ ? decl_->isClassDeclaration() : NULL;
}


FuncDeclaration* IRState::hasFuncDecl()
{
    return decl_ ? decl_->isFuncDeclaration() : NULL;
}


VarDeclaration* IRState::hasVarDecl()
{
    return decl_ ? decl_->isVarDeclaration() : NULL;
}


/*********************/
#pragma region toElem

elem* IRState::toElem(AddExp& exp)
{
    return createBinaryOp(exp, IL_add);
}


elem* IRState::toElem(AddAssignExp& exp)
{
    return createAssignOp(exp, IL_add);
}


elem* IRState::toElem(AddrExp& exp)
{
    // the type of this expression is pointer, loadExp
    // will convert it to address so we do not need to
    // do anything special here
    return &exprToElem(*this, DEREF(exp.e1));
}


elem* IRState::toElem(AndAndExp& exp)
{
    return createLogicalOp(exp, IL_and);
}


elem* IRState::toElem(AndAssignExp& exp)
{
    return createAssignOp(exp, IL_and);
}


elem* IRState::toElem(AndExp& exp)
{
    return createBinaryOp(exp, IL_and);
}


//Generate code to load the length of a static or dynamic,
//(non-associative) array on the evaluation stack
static elem* arrayLen(IRState& irstate,
                      Expression& exp,
                      Loc& loc,
                      Type* type
                      )
{
    Variable* v = NULL;
    elem* e = &loadExp(irstate, exp, &v);
    if (v)
    {
        e = new ArrayLen(*v, *e, type);
        irstate.add(*e);
    }
    else
    {
        BackEnd::fatalError(loc, "expression cannot be referenced");
    }
    return e;
}


elem* IRState::toElem(ArrayLengthExp& exp)
{
    return arrayLen(*this, DEREF(exp.e1), exp.loc, exp.type);
}


elem* IRState::toElem(ArrayLiteralExp& exp)
{
    ArrayType& aType = getArrayType(toILType(exp.loc, exp.type));
    block& blk = *getBlock().addNewBlock(&exp.loc);
    IRState state(blk);
    VarArgs* args = VarArgs::create(state, exp.elements, exp.loc, 0, false);
    elem* e = blk.add(*args);

    if (inCall_ && inCall_->convertArrays())
    {
        // handle the case when array literals are passed as arguments
        // to functions, and the arrays need to be converted to slices
        e = blk.add(*new ArrayToSlice(aType, exp.loc));
    }
    else
    {
        SliceFiller* filler = new SliceFiller(args->size());
        e = blk.add(*filler);
        filler->setElemTYPE(aType.elemTYPE());
    }
    return init(*this, exp, e);
}


static elem* handleInvariant(IRState& irstate, AssertExp& exp)
{
    elem* result = NULL;

    if (global.params.useInvariants)
    {
        AggregateDeclaration* decl = NULL;
        TYPE& type = toILType(exp.loc, DEREF(exp.e1).type);
        if (ObjectType* objType = type.isObjectType())
        {
            decl = objType->getClassDecl();
        }
        else if (PointerType* pType = type.isPointerType())
        {
            if (StructType* sType = pType->getPointedType().isStructType())
            {
                decl = sType->getStructDecl();
            }
        }
        if (decl)
        {
            if (InvariantDeclaration* inv = decl->inv)
            {
                Method& method = toMethod(*inv);
                result = CallInstruction::create(irstate.getBlock(), method, &exp.loc);
            }
        }
    }
    return result;
}


elem* IRState::toElem(AssertExp& exp)
{
    block* blk = getBlock().addNewBlock(exp.loc);

    if (global.params.useAssert)
    {
        //create a block to jump to if the assertion holds
        block* assertOK = getBlock().addNewBlock(exp.loc);
        EmitInBlock group(*this, *blk);
        elem* condition = &loadExp(*this, DEREF(exp.e1));
        if (!handleInvariant(*this, exp))
        {
            Branch::create(condition, getBlock(), assertOK);

            string loc = exp.loc.toChars();
            replaceBackSlashes(loc);
            string msg = "Assertion failed at: " + loc;
            if (exp.msg)
            {
                msg += ": ";
            }
            new StringLiteral(getBlock(), msg, e_SystemString);

            Temporary<Encoding> setEncoding(encoding_, e_SystemString);
            if (exp.msg)
            {
                exp.msg->toElem(this);
                add(*new StringConcat);
            }
            AssertError::create(getBlock());
            Instruction::create(getBlock(), IL_throw, &exp.loc);
        }
    }
    return blk;
}


elem* IRState::toElem(AssocArrayLiteralExp& exp)
{
    AssocArrayType& aaType = getAssocArrayType(toILType(exp.loc, exp.type));
    block& blk = *getBlock().addNewBlock(&exp.loc);
    elem* result = blk.add(*NewAssociativeArray::create(aaType));
    IRState state(blk);
   
    TYPE* keyTYPE = &aaType.keyTYPE();
    VarArgs* keys;
    {
        Temporary<Encoding> setEncoding(state.encoding_, e_SystemString);
        keys = VarArgs::create(state, exp.keys, exp.loc, 0, false, keyTYPE);
    }
    blk.add(*keys);

    VarArgs* values = VarArgs::create(state, exp.values, exp.loc, 0, false);
    blk.add(*values);

    AssocArrayLiteral* aalit = new AssocArrayLiteral(aaType);
    blk.add(*aalit);

    return init(*this, exp, result);
}


#pragma region Array Helpers
static bool initDArray(block& blk, Loc& loc, Variable& a, SliceFiller& fill)
{
    assert(fill.length());
    const ArrayType& aType = getArrayType(toILType(loc, a.getType()));
    return aType.initDynamic(blk, loc, a, fill.length());
}


#pragma region ArrayCopy Helpers
// Helpers for copying element ranges from one array to another
namespace {
    class ArrayCopy4 : public elem
    {
        int32_t getStackDelta() const { return -4; }

        elem* emitILAsm(wostream& out)
        {
            indent(out) << "call void [dnetlib]runtime.Array::Copy(\n"
                            "\t\tclass [mscorlib]System.Array,\n"
                            "\t\tint32, int32,\n"
                            "\t\tclass [mscorlib]System.Array)\n";
            return this;
        }
    };


    class ArrayCopy6 : public elem
    {
        int32_t getStackDelta() const { return -6; }

        elem* emitILAsm(wostream& out)
        {
            indent(out) << "call void [dnetlib]runtime.Array::Copy(\n"
               "\t\tclass [mscorlib]System.Array, int32, int32,\n"
               "\t\tclass [mscorlib]System.Array, int32, int32)\n";
            return this;
        }
    };
}
#pragma endregion ArrayCopy Helpers


void IRState::arrayAssign(AssignExp& exp, Variable& lhs, elem& rhs, ArrayType& aType)
{
    ASlice* aslice = getBlock().back()->isASlice();
    if (aslice)
    {
        // in assignments the RHS child is visited first, the slice
        // must go before the RHS (which may be an array literal or
        // a numeric value to be blasted into all array elems -- in 
        // which case exp.ismemset is true)
        getBlock().swapTail();
    }
    SliceFiller* sliceFiller = rhs.isSliceFiller();
    if (exp.ismemset)
    {
        assert(!sliceFiller);
        assert(aslice);
        TYPE& rhsType = toILType(exp.e2->loc, exp.e2->type);
        if (!rhsType.isObjectType())
        {
            Box::create(getBlock(), rhsType);
        }
        sliceFiller = new SliceFiller(0, aslice);
        add(*sliceFiller);
    }
    else if (sliceFiller)
    {
        if (aslice) sliceFiller->setSlice(aslice);
        if (sliceFiller->length())
        {
            // make sure that the left hand-side array is large
            // enough to accommodate all elements in the rhs slice
            if (initDArray(getBlock(), exp.loc, lhs, *sliceFiller))
            {
                //IF_DEBUG(rhs.getOwner()->emitIL(wclog));
                block* b = DEREF(rhs.getOwner()).getOwner();
                assert(b);
                assert(b->back() == rhs.getOwner());
                // move fill after newarr
                b->pop_back(false);
                getBlock().add(*rhs.getOwner());
            }
        }
    }
    else if (aslice)
    {
        if (ASlice* other = rhs.isASlice())
        {
            rhs.getOwner()->pop_back(); // discard slice->load()
            getBlock().swapTail();
            if (aslice->lower() == 0)
            {
                assert(aslice->upper() == 0);
                add(*new ArrayCopy4);
            }
            else
            {
                add(*new ArrayCopy6);
            }
        }
        else
        {
            sliceFiller = new SliceFiller(0, aslice, true);
            add(*sliceFiller);

            //hack: ensure that a future setType triggers a type
            //check that will prevent morphing the 'v' array into a slice
            lhs.setType(lhs.getType(), exp.loc);
        }
    }
    else
    {
        if (ASlice* aslice = rhs.isASlice())
        {
            aslice->getOwner()->pop_back(); //todo: revisit this hack
            getBlock().add(*NewSlice::create(aType));
            lhs.setType(DEREF(aType.getSliceType(exp.loc)).getType(), exp.loc);
        }
        //the right hand-side may be an array slice (even if not ASlice block)
        else if (Variable* rhsVar = rhs.isVariable())
        {
            if (!toILType(exp.loc, rhsVar->getType()).isSliceType()
                && aType.isSliceType()
                )
            {
                assert(toILType(exp.loc, rhsVar->getType()).isArrayType());
                getBlock().add(*new ArrayToSlice(aType, exp.loc));
            }
            else
            {
                //transfer the type info to the left hand-side
                lhs.setType(rhsVar->getType(), exp.loc);
            }
        }
        lhs.store(getBlock(), &exp.loc);
    }
    if (sliceFiller)
    {
        Instruction::create(getBlock(), IL_pop);    //discard Slice::Fill result
    }
}
#pragma endregion Array Helpers


static void initStruct(IRState& irstate, 
                       Loc& loc,
                       TYPE& t,
                       Variable& v,
                       bool load = false
                       )
{
#if !defined( VALUETYPE_STRUCT )
    StructType* st = t.isStructType();
    if (!st)
    {
        BackEnd::fatalError(loc, "struct type expected");
    }
    if (v.getDecl().ident != Id::This)
    {
        new DefaultCtorCall<StructType>(irstate.getBlock(), IL_newobj, *st);
    }
    v.store(irstate.getBlock(), &loc);
    if (load)
    {
        v.load(irstate.getBlock());
    }
#endif
}


static inline void loadString(block& blk, Expression& exp, bool deref = false)
{
    Const<size_t>::create(*TYPE::Uint, 0, blk, exp.loc);
    ArrayElemAccess::load(blk, DEREF(TYPE::Char), deref);
}


elem* IRState::toElem(AssignExp& exp)
{
    block* slot = getBlock().addNewBlock();
    elem* rhs = NULL;
    block* rhsBlock = new block(NULL, exp.loc, getBlock().depth());
    {
        EmitInBlock group(*this, *rhsBlock);
        rhs = &loadExp(*this, DEREF(exp.e2));
    }
    elem& lhs = exprToElem(*this, DEREF(exp.e1));

    if (Variable* v = lhs.isVariable())
    {
        VarDeclaration& vdecl = v->getDecl();
        verifyForeachRef(exp.loc, vdecl, foreachAggr());
        if (vdecl.isRef())
        {
            //ensure the order of operands on the stack
            //expected by a "store indirect" instruction
            Temporary<unsigned> storage(vdecl.storage_class);
            vdecl.storage_class &= ~STCref;
            v->load(getBlock(), &exp.loc);
            // load the right hand-side AFTER the lhs
            add(*rhsBlock);
        }
        else
        {
            // load the right hand-side BEFORE the lhs
            slot->add(*rhsBlock);
        }

        TYPE& lhsTYPE = toILType(exp.loc, v->getType());
        ArrayType* arrayType = lhsTYPE.isArrayType();
        if (arrayType && !hasVarDecl())
        {
            // handle array assignment in non decl
            arrayAssign(exp, *v, DEREF(rhs), *arrayType);
        }
        else
        {
            assert(exp.type == exp.e1->type);

            if (arrayType)
            {   // slice declaration? a[lwr..upr] is expected to follow
                if (arrayType->isSliceType())
                {
                    add(*NewSlice::create(*arrayType));
                }
                else if (Variable* rhsVar = rhs->isVariable())
                {
                    // if it is an array assignment that occurs in the context
                    // of a declaration, we may need to transfer the type info
                    // from the right hand-side to the left, if assigning a slice
                    //
                    //int b[] = a[2..5]; // don't set type, handled in toElem(SliceExp&)
                    //int c[] = b;       // must transfer type info from b to c
                    if (toILType(exp.loc, rhsVar->getType()).isArrayType())
                    {
                        v->setType(rhsVar->getType(), exp.loc);
                    }
                }
                else if (exp.e2->op == TOKnew)
                {
                    v->setType(exp.e2->type, exp.loc);
                }
            }

            // struct decl initialized from literal?
            if ((exp.e2->op == TOKstructliteral) && hasVarDecl())
            {
#if VALUETYPE_STRUCT
                assert(rhs->isBlock());
                v->load(getBlock(), &exp.loc, true);
#else
                if (VarDeclaration* vdecl = hasVarDecl())
                {
                    EmitInBlock initGroup(*this, *getBlock().addNewBlock());
                    initStruct(*this, exp.loc, lhsTYPE, *v, true);
                }
#endif
                getBlock().swapTail();
            }
            else if (DEREF(v->getType()).ty == Tstruct)
            {   // default struct initialization may generate
                // assignment from zero, discard it; note that
                // struct literal initializer do not fall under
                // this case
                if (DEREF(exp.e2).isConst())
                {
                    assert (DEREF(exp.e2).toInteger() == 0);
                    Instruction::create(getBlock(), IL_pop);
                }
                //are we in the context of declaring 'v'?
                else if (hasVarDecl() == &v->getDecl())
                {
                    if (exp.op == TOKconstruct)
                    {
                        v->store(getBlock(), &exp.loc);
                    }
                    else
                    {
                        initStruct(*this, exp.loc, lhsTYPE, *v);
                    }
                }
                else
                {
#if VALUETYPE_STRUCT
                    if ((exp.e2->op == TOKthis) && lhsTYPE.isStructType())
                    {
                        v->load(getBlock(), &exp.loc);
                        return add(*new StoreIndirect(lhsTYPE));
                    }
#endif
                    v->store(getBlock(), &exp.loc);
                }
            }
            else
            {
                //deal with statements such as:
                //invariant char* p = "ABC".ptr;
                //work around the front-end optimizing .ptr away
                if ((exp.e2->op == TOKstring) && lhsTYPE.isPointerType())
                {
                    loadString(getBlock(), exp, true /* deref */);
                }
#if !VALUETYPE_STRUCT
                if (lhsTYPE.isStructType())
                {
                    v->load(getBlock());
                    getBlock().swapTail();
                    return add(*new StoreIndirect(lhsTYPE, true));
                }
#endif
                //assigning ref to ref involves no redirect
                if (v->isRefType())
                {
                    //move the contents of rhsBlock away to a block that
                    //never gets emitted -- it is not safe to call rhsBlock->clear()
                    //since the elements in rhsBlock may be referenced by other elems
                    rhsBlock->moveElemsTo(BackEnd::instance().getAssembly());
                    EmitInBlock group(*this, *rhsBlock);
                    &loadExp(*this, DEREF(exp.e2), false, true);
                }

                v->store(getBlock(), &exp.loc);
            }
        }
    }
    return &lhs;
}


elem* IRState::toElem(BoolExp& exp)
{
    return DEREF(exp.e1).toElem(this);
}


//In D, arrays, slices and strings can be used interchangeably (to some degree)
//but in the .NET backend they are implemented using different System classes:
//Array, ArraySegment, and String, respectively.
SliceType* loadArray(block& blk, Variable& v, bool inSliceExp)
{
    TYPE& t = toILType(v);
    if (SliceType* sliceType = t.isSliceType())
    {
        v.load(blk, NULL, true);
        blk.add(*PropertyGetter::create(t, "Array", "!0[]"));
        return sliceType;
    }
    else
    {
        v.load(blk);
    }
    return NULL;
}


SliceType* loadArray(IRState& irstate, Variable& v, bool inSliceExp)
{
    return loadArray(irstate.getBlock(), v, inSliceExp);
}


static void loadArray(IRState& irstate, elem* e)
{
    if (Variable* v = e->isVariable())
    {
        loadArray(irstate, *v);
    }
}


bool isSysProperty(Method& method)
{
    Declaration& decl = method.getDecl();
    if (!decl.isMember()
        && method.getArgCount() == 1
        && decl.ident == Lexer::idPool("sys"))
    {
        return true;
    }
    return false;
}


///<summary>
///Emit code that loads a closure object on the evaluation stack.
///Assumption: method is nested inside the current function.
///</summary>
void loadClosure(block& blk, Loc& loc, Method* method)
{
    assert(method->isNested());

    Method& currentFunc = BackEnd::getCurrentMethod(loc);
    AggregateDeclaration& aggrDecl = DEREF(method->getAggregateDecl());
    Type* type = aggrDecl.type;     //the type of the closure
    // caller and callee share the same class?
    const bool sameClass = (currentFunc.getDecl().parent == &aggrDecl);
#if VALUETYPE_STRUCT
    if (DEREF(type).ty == Tstruct)
    {
        type = type->pointerTo();
    }
#endif
    Identifier* ident = Identifier::generateId("$closure");
    VarDeclaration* vdecl = new VarDeclaration(loc, type, ident, NULL);
    Variable* temp = currentFunc.addLocal(*vdecl);
    if (sameClass)
    {
        blk.add(*new ThisArg);
    }
    else
    {
        new DefaultCtorCall<AggregateDeclaration>(blk, IL_newobj, aggrDecl);
    }
    if ((&currentFunc != method) // recursive call?
        && (sameClass || aggrDecl.parent == &currentFunc.getDecl())
        )
    {
        temp->store(blk);
        // copy variables from the caller's lexical scope to the closure object
        Closure* closure = method->getClosure();
        DEREF(closure).copyIn(blk, temp);
        temp->load(blk);
    }
}



static Variable* getThisArg(FuncDeclaration& decl)
{
    Variable* v = NULL;
    if (decl.vthis)
    {
        if (Symbol* sym = decl.vthis->csym)
        {
            v = sym->isVariable();
        }
    }
    if (!v)
    {
        BackEnd::fatalError(decl.loc, "function has no `this' argument");
    }
    return v;
}



static void ensureCtorGuard(IRState& irstate, Method& method, Loc& loc)
{
    assert(method.isCtor()); // pre-condition

    if (FuncDeclaration* fdecl = irstate.hasFuncDecl())
    {
        //is this method call inside of a ctor?
        if (fdecl->isCtorDeclaration())
        {
            if (Class* c = method.getClass())
            {
                //do the caller and callee ctors belong to same class?
                if (&c->getDecl() == fdecl->isClassMember())
                {
                    method.setDelegatedTo();
                    block& blk = irstate.getBlock();
                    //set the $in_ctor flag to true in the generated code
                    Const<bool>::create(*TYPE::Boolean, true, blk, loc);
                    blk.swapTail();
                    Variable& guard = c->getCtorDelegationGuard();
                    guard.store(blk);
                    blk.add(*new ThisArg);
                }
            }
        }
    }
}


namespace {
    ///<summary>
    /// Invoke lazy argument assuming that it was passed in as a delegate
    ///</summary>
    class LazyInvoke : public elem
    {
        elem* emitILAsm(wostream& out)
        {
            indent(out) << "ldnull\n";
            indent(out) << "callvirt instance object [mscorlib]System.Delegate::DynamicInvoke(object[])\n";
            return this;
        }

        int32_t getMaxStackDelta() const
        {
            return 1;
        }
    };
}


static elem* isLazyEval(IRState& irstate, CallExp& exp, elem& e)
{
    elem* result = NULL;

    if (Variable* v = e.isVariable())
    {
        if (v->getDecl().storage_class & STClazy)
        {
            block& blk = irstate.getBlock();
            v->load(blk, &exp.loc);
            irstate.add(*new LazyInvoke);

            if (DEREF(v->getType()).ty != Tvoid)
            {
                TYPE& argTYPE = toILType(exp.loc, v->getType());
                Conversion::unbox(blk, argTYPE, &exp.loc);
            }
            result = savePartialResult(blk, v->getDecl(), v->getType());
        }
    }
    return result;
}


static elem* postCall(IRState& irstate, CallExp& exp, Method& method, elem* result)
{
    Variable* v = result->isVariable();
    if (v)
    {
        block& blk = irstate.getBlock();
        if (method.needsConversion())
        {
            v->load(blk);
            Conversion::create(blk, toILType(exp.loc, method.getRetType()));

            //CallInstruction::create calls savePartialResult internally, but
            //after converting or dereferencing we need to wrap again, so that if 
            //the result is never used, the result is not left on the stack.
            result = v = savePartialResult(irstate, v->getDecl(), method.getRetType());
        }
    }
    if (method.getDecl().ident == Id::assign)
    {
        if (Variable* obj = method.getObject())
        {
            if (PartialResult* temp = obj->isPartialResult())
            {
                temp->suppress();
                if (obj->isBound())
                {
                    if (!v)
                    {
                        v = obj;
                    }
                    v->load(irstate.getBlock());
                    obj->store(irstate.getBlock());
                }
            }
        }
    }
    return result;
}


elem* IRState::toElem(CallExp& exp)
{
    Temporary<Method*> callScope(inCall_);
    // A function call will most likely result in several IL statements,
    // for loading the arguments on evaluation stack, and the call proper;
    // group everything in a block, so that if we need to call swapTail()
    // later, it will handle atomically all code generated here.
    block* blk = getBlock().addNewBlock(exp.loc);
    EmitInBlock group(*this, *blk);

    //generate the code for the called expression
    elem& e = exprToElem(*this, DEREF(exp.e1));
    
    if (elem* lazy = isLazyEval(*this, exp, e))
    {
        return lazy;
    }

    Method& method = prepCall(exp.e1->loc, exp.arguments, e.isSymbol());
    if (method.isOptimizedOut())
    {
        method.optimizeOut(false);
        return &e;
    }
    if (method.isCtor())
    {
        ensureCtorGuard(*this, method, exp.loc);
    }
    //create the element that emits the CALL (or CALLVIRT) instruction
    elem* result = CallInstruction::create(getBlock(), method, &exp.loc);

    result = postCall(*this, exp, method, result);
    return result;
}


elem* IRState::toElem(CastExp& exp)
{
    Variable* v = NULL;
    loadExp(*this, DEREF(exp.e1), &v);

    //if (DEREF(exp.e1).type->implicitConvTo(exp.to))
    //{
    //    return getBlock().back();
    //}
    TYPE& toType = toILType(exp.loc, exp.to);
    TYPE& fromType = v
        ? toILType(exp.loc, v) 
        : toILType(exp.loc, DEREF(exp.e1).type);

    if (ArrayType* to = toType.isArrayType())
    {
        if (ArrayType* from = fromType.isArrayType())
        {
            if (&to->elemType() == &from->elemType())
            {
                assert(getBlock().back() != v);
                getBlock().pop_back();
                return v;
            }
        }
    }
    // for implicit conversions of variadic functions _arguments
    if (ObjectType* objType = fromType.isObjectType())
    {
        if (ClassDeclaration* cd = objType->getClassDecl())
        {
            if (cd->ident == Id::object
             || cd->ident == Lexer::idPool("[mscorlib]System.Object"))
            {
                //unboxing a [dnetlib]core.Object from System.Object
                //will not work, it is best to leave it alone
                if (exp.to == ClassDeclaration::object->type)
                {
                    return getBlock().back();
                }
                return Conversion::unbox(getBlock(), toType, &exp.loc);
            }
        }
    }

    if (v && toType.isPointerType()) //casting from array to ptr?
    {
        if (ArrayType* fromArray = fromType.isArrayType())
        {
            if (fromArray->isSliceType())
            {
                getBlock().pop_back();
                v->load(getBlock(), &exp.loc, true);
                add(*PropertyGetter::create(*fromArray, "Array", "!0[]"));
                v->load(getBlock(), &exp.loc, true);
                add(*PropertyGetter::create(*fromArray, "Offset", "int32"));
            }
            else
            {
                Const<size_t>::create(*TYPE::Uint, 0, getBlock(), exp.loc);
            }
            return ArrayElem::create(getBlock(), v->getDecl(), &fromArray->elemType());
        }
    }
    // a conversion from slice to array may occur because of
    // our changing array types to slices on the fly -- the cast
    // generated by the front-end might have been before related
    // array types
    else if (fromType.isSliceType() && !toType.isSliceType())
    {
        if (v)
        {
            VarBlockScope varScope(*this, exp.loc);
            Method& method = varScope.getMethod();
            v = method.addLocal(v->getDecl(), v->getType());
            v->store(getBlock());
            return v;
        }
        else
        {
            BackEnd::fatalError(exp.loc, "unhandled cast from slice to array");
        }
    }
    if (!fromType.isObjectType() && (exp.to == ClassDeclaration::object->type))
    {
        return Box::create(getBlock(), fromType, &exp.loc);
    }

    return Conversion::create(getBlock(), toType, &exp.loc);
}


elem* IRState::toElem(CommaExp& exp)
{
    exprToElem(*this, DEREF(exp.e1));
    return &exprToElem(*this, DEREF(exp.e2));
}


namespace {
    ///<summary>
    ///generate a [dnetlib]runtime.Array::Concat call
    ///</summary>
    class ArrayConcat : public elem
    {
        ArrayType& aType_;
        TYPE& argType_;

        //pops two args, pushes result
        int32_t getStackDelta() const { return -1; }

        void emitArgs(wostream& out)
        {
            if (argType_.isSliceType())
            {
                out << ", $SLICE<!!0>)\n";
            }
            else if (ArrayType* a = argType_.isArrayType())
            {
                if (&a->elemTYPE() == &aType_.elemTYPE())
                {
                    out << ", !!0[])\n";
                }
                else
                {
                    out << ", !!0)\n";
                }
            }
            else
            {
                out << ", !!0)\n";
            }
        }
        elem* emitILAsm(wostream& out)
        {
            if (aType_.isSliceType())
            {
                indent(out) << "call $SLICE<!!0> [dnetlib]runtime.Slice::Concat<";
                out << aType_.elemTYPE().name() << ">($SLICE<!!0>";
                emitArgs(out);
            }
            else
            {
                indent(out) << "call !!0[] [dnetlib]runtime.Array::Concat<";
                out << aType_.elemTYPE().name() << ">(!!0[]";
                emitArgs(out);
            }
            return this;
        }

    public:
        ArrayConcat(ArrayType& aType, TYPE& argType)
            : aType_(aType), argType_(argType)
        { }
    }; //ArrayConcat
}


///<summary>
///Generate code for concatenating arrays, called by toElem(CatAssignExp&)
///and toElem(CatExp&)
///</summary>
static elem* arrayConcat(IRState& irstate, BinExp& exp, bool assign = false)
{
    elem* result = NULL;
    block& blk = irstate.getBlock();

    //load the left hand-side of the concat expression
    Variable* v = NULL;
    loadExp(irstate, DEREF(exp.e1), &v);

    Type* lhsType = v ? v->getType() : DEREF(exp.e1).type;
    if (ArrayType* aType = toILType(exp.loc, lhsType).isArrayType())
    {
        //get the right hand-side on the stack
        Variable* arg = NULL;
        elem& e = loadExp(irstate, DEREF(exp.e2), &arg);
        TYPE& argType = toILType(exp.loc, arg ? arg->getType() : DEREF(exp.e2).type);
        if (e.isSliceFiller())
        {
            assert(exp.e2->op == TOKarrayliteral);
            // handle the case when e2 is an array literal
            e.getOwner()->pop_back();
        }
        //add an element that emits a call to the Array::Concat runtime helper
        result = irstate.add(*new ArrayConcat(*aType, argType));
        if (v)
        {
            if (assign)
            {
                //The result of the concatenation must now be stored back
                //into the left hand-side array, which may be the field
                //of a struct, etc.: save the partial result here, so that
                //the v->store() operation can rearrange instructions as needed.
                Variable* tmp = savePartialResult(irstate, v->getDecl(), v->getType(), false);
                tmp->load(blk, &exp.loc);

                result = v;
                v->store(blk);
            }
            else
            {
                result = savePartialResult(irstate, v->getDecl(), lhsType);
            }
        }
    }
    else
    {
        BackEnd::fatalError(exp.loc, "expression does not yield array type");
    }
    return result;
}


elem* IRState::toElem(CatAssignExp& exp)
{
    return arrayConcat(*this, exp, true);
}


elem* IRState::toElem(CatExp& exp)
{
    return arrayConcat(*this, exp);
}


elem* IRState::toElem(CmpExp& exp)
{
    genBinaryArgs(exp);

    switch (exp.op)
    {
    case TOKlt:
        return Instruction::create(getBlock(), IL_clt, &exp.loc);
    case TOKgt:
        return Instruction::create(getBlock(), IL_cgt, &exp.loc);
    case TOKle:
        Instruction::create(getBlock(), IL_cgt, &exp.loc);
        return LogicalNegation::create(getBlock(), &exp.loc);
    case TOKge:
        Instruction::create(getBlock(), IL_clt, &exp.loc);
        return LogicalNegation::create(getBlock(), &exp.loc);
    }
    throw logic_error("unhandled comparison operator");
}


elem* IRState::toElem(CondExp& exp)
{
    block* b1 = new block(&getBlock(), exp.loc, getBlock().depth());
    block* done = new block(&getBlock(), exp.loc, getBlock().depth());

    elem* cond = &exprToElem(*this, DEREF(exp.econd));
    // conditional branch to first expression
    Branch::create(cond, getBlock(), b1);
    block* b2 = getBlock().addNewBlock(&exp.loc);
    {
        EmitInBlock exp2Scope(*this, *b2);
        loadExp(*this, DEREF(exp.e2));
        Branch::create(*b2, done);
    }
    {
        EmitInBlock exp1Scope(*this, *b1);
        loadExp(*this, DEREF(exp.e1));
    }
    add(*b1);
    return add(*done);
}


elem* IRState::toElem(DeclarationExp& exp)
{
    DEREF(exp.declaration).toObjFile(0);
    return exp.declaration->csym;
}


elem* IRState::toElem(DelegateExp& exp)
{
    elem& obj = loadExp(*this, DEREF(exp.e1));
    Method& method = toMethod(DEREF(exp.func));
    if (method.isNested())
    {
        assert(!obj.isVariable());
        loadClosure(getBlock(), exp.loc, &method);
    }
    if (method.funcDecl().isVirtual())
    {
        Instruction::create(getBlock(), IL_dup);
    }
    LoadFunc::create(getBlock(), method, &exp.loc);
    return add(*new Delegate(toILType(exp.loc, exp.type)));
}


elem* IRState::toElem(DeleteExp& exp)
{
    //generate a call to Object::Dispose (the root object implements IDisposable)
    class Dispose : public elem
    {
        int32_t getStackDelta() const { return -1; } // pops one, returns void

        elem* emitILAsm(wostream& out)
        {
            indent(out) << "call instance void [dnetlib]core.Object::Dispose()\n";
            return this;
        }
    }; //Dispose

    loadExp(*this, DEREF(exp.e1));
    return getBlock().add(*new Dispose);
}


elem* IRState::toElem(DivAssignExp& exp)
{
    return createAssignOp(exp, IL_div);
}


elem* IRState::toElem(DivExp& exp)
{
    return createBinaryOp(exp, IL_div);
}


elem* IRState::toElem(DotTypeExp& exp)
{
    return DEREF(exp.e1).toElem(this);
}


elem* IRState::toElem(DotVarExp& exp)
{
    // group the both sides of the dot into one elem
    block* blk = getBlock().addNewBlock(exp.loc);
    EmitInBlock group(*this, *blk);

    bool byAddr = false;
#if VALUETYPE_STRUCT
    if (DEREF(exp.e1).type->ty == Tstruct)
    {
        byAddr = true;
    }
#endif
    Symbol* sym = DEREF(exp.var).toSymbol();    // member

    Variable* v = NULL;
    loadExp(*this, DEREF(exp.e1), &v, byAddr);  // owner object

    if (!v)
    {
        VarDeclaration* vd = new VarDeclaration(exp.loc, exp.e1->type, NULL, NULL);
        v = savePartialResult(*this, *vd, false);
        v->load(getBlock());
    }
    if (sym)
    {
        sym->bind(v);
    }
    return sym;
}




/**************************************************************
 * Helpers for EqualExp and IdentityExp.
 */
namespace {
    ///<summary>
    ///generate a [dnetlib]runtime.Array::Equals call
    ///</summary>
    class ArrayEquals : public elem
    {
        TYPE& lhsType_;
        TYPE& rhsType_;
        bool shallow_;

        void emitArgType(wostream& out, TYPE& type, bool isLHS = false)
        {
            if (SliceType* stype = type.isSliceType())
            {
                if (isLHS)
                {
                    out << "<" << stype->elemTYPE().name() << ">(";
                }
                out << "$SLICE<!!0>";
            }
            else
            {
                if (isLHS)
                {
                    out << "(";
                }
                out << "class [mscorlib]System.Array";
            }
        }

        //pops two args, pushes result
        int32_t getStackDelta() const { return -1; }

        elem* emitILAsm(wostream& out)
        {
            indent(out) << "call bool [dnetlib]runtime.Array::";
            if (shallow_)
            {
                out << "Equiv";
            }
            else
            {
                out << "Equals";
            }
            emitArgType(out, lhsType_, true);
            out << ", ";
            emitArgType(out, rhsType_);
            out << ")\n";
            return this;
        }

    public:
        ArrayEquals(TYPE& lhsType, TYPE& rhsType, bool shallow = false) 
            : lhsType_(lhsType)
            , rhsType_(rhsType)
            , shallow_(shallow)
        {
            assert(lhsType.isArrayType());
            assert(rhsType.isArrayType());
        }
    };


    // Emits call to Equals() (for structs).
    // For struct objects, identity is defined as the bits in the struct
    // being identical, so this should work for both equality and identity.
    // todo: the default implementation of Equals for struct value types
    // uses reflection (slow); we should be better off synthesizing our
    // own Equals.
    class EqualsCall : public elem
    {
        TYPE& type_;

        elem* emitILAsm(wostream& out)
        {
            // the nonvirtual call is executed on the this pointer
            indent(out) << "constrained. " << type_.name() << '\n';
            indent(out) << "callvirt instance bool object::Equals(object)\n";
            return this;
        }
        int32_t getStackDelta() const
        {
            return -1; // pops 'this', 'object', returns bool
        }

    public:
        explicit EqualsCall(TYPE& type) : type_(type)
        {
        }
    };


    template<TOK TEqual, TOK TNotEqual, bool Identity, typename E>
    elem* generateEqualOrIdentity(IRState& irstate, E& exp)
    {
        Type* lhsType = DEREF(exp.e1).type;
        Type* rhsType = DEREF(exp.e2).type;

        // is the left hand-side expression of a struct type?
        const bool lhsStruct = DEREF(lhsType).ty == Tstruct;
        
        Variable* lval = NULL;
        Variable* rval = NULL;
        loadExp(irstate, DEREF(exp.e1), &lval, lhsStruct);
        loadExp(irstate, DEREF(exp.e2), &rval);

        elem* result = NULL;
        block& blk = irstate.getBlock();
        // (1) scalars
        if (DEREF(lhsType).isscalar() || DEREF(rhsType).isscalar())
        {
            assert(lhsType->isscalar());
            assert(rhsType->isscalar());
            // generate compares-equal instruction
            result = Instruction::create(irstate.getBlock(), IL_ceq, &exp.loc);
        }
        // (2) structs
        else if (lhsStruct || (DEREF(rhsType).ty == Tstruct))
        {
            assert(lhsType->ty == Tstruct);
            assert(rhsType->ty == Tstruct);
            Box::create(blk, toILType(exp.loc, rhsType));
            // generate code that calls  Equals
            result = blk.add(*new EqualsCall(toILType(exp.loc, lhsType)));
        }
        else // (3) arrays
        {
            TYPE& lhsTYPE = toILType(exp.loc, lval ? lval->getType() : lhsType);
            TYPE& rhsTYPE = toILType(exp.loc, rval ? rval->getType() : rhsType);

            result = blk.add(*new ArrayEquals(lhsTYPE, rhsTYPE, Identity));
        }
        switch (exp.op)
        {
        case TEqual:
            break;

        case TNotEqual:
            result = LogicalNegation::create(blk, &exp.loc);
            break;

        default:
            BackEnd::fatalError(exp.loc, "unhandled equal or identity operator");
        }
        return result;
    }
}


// Generate code for comparing scalars, structs and arrays.
// The front-end translates object comparisons into opEquals calls so we
// do not need to worry about them here.
elem* IRState::toElem(EqualExp& exp)
{
    return generateEqualOrIdentity<TOKequal, TOKnotequal, false>(*this, exp);
}


///<summary>
///Generate function from FuncLiteralDeclaration
///</summary>
static void makeFunc(ModuleEmitter& mod, block& blk, FuncDeclaration& fdecl)
{
    mod.createFunction(fdecl);
    assert(fdecl.csym); //createMethod post-condition
    Method& m = toMethod(fdecl);
    if (m.isNested())
    {
        loadClosure(blk, fdecl.loc, &m);
    }
}


elem* IRState::toElem(FuncExp& exp)
{
    if (DEREF(exp.fd).csym == NULL)
    {
        ModuleEmitter& mod = BackEnd::getModuleScope(exp.loc);
        makeFunc(mod, getBlock(), DEREF(exp.fd));
    }
    Method& m = toMethod(DEREF(exp.fd));
    return LoadFunc::create(getBlock(), m, &exp.loc);
}


//Generate code that checks for out-of-bounds access in an IndexExp
//The slice (ArraySegment) object is in a temporary variable
static void generateUpperBoundCheck(IRState& irstate,
                                    Loc& loc,
                                    SliceType& sliceType,
                                    Variable& temp
                                    )
{
    block* blk = &irstate.getBlock();
    //where to resume flow if everything checks out
    block* cont = new block(blk, loc, blk->depth());

    blk = blk->addNewBlock(loc);
    //assume the index is on top of the stack, duplicate it
    //so that it can be consumed by a comparison operation
    Instruction::create(*blk, IL_dup);

    //load the slice size on the stack
    temp.load(*blk, &loc, true);
    blk->add(*PropertyGetter::create(sliceType, "Count", "int32"));
    //branch to continuation block if index is lower than count
    elem* br = BranchInstruction::create(*blk, IL_blt);
    Branch::create(br, *blk, cont);

    blk = irstate.getBlock().addNewBlock(loc);
    IndexOutOfRangeException::create(*blk);
    Instruction::create(*blk, IL_throw);
    irstate.add(*cont);
    irstate.setBlock(*cont);
}


///<summary>
/// Loads on the evaluation stack the y part of a x[y] expression.
/// Used for both "regular" and associative arrays.
///</summary>
static Variable* loadArrayIndex(IRState& irstate,
                                VarBlockScope& varScope,
                                Loc& loc,
                                Variable& arrayVar,
                                TYPE& type, // back-end type of arrayVar
                                Expression* indexExp,
                                Variable* indexVar = NULL
                                )
{
    Variable* temp = NULL;
    Variable* v = NULL;
    SliceType* sliceType = type.isSliceType();
    if (sliceType)
    {
        Method& method = varScope.getMethod();
        temp = method.addLocal(arrayVar.getDecl(), arrayVar.getType());
        temp->store(irstate.getBlock());
        temp->load(irstate.getBlock(), &loc, true);
        irstate.add(*PropertyGetter::create(*sliceType, "Array", "!0[]"));
        temp->load(irstate.getBlock(), &loc, true);
        irstate.add(*PropertyGetter::create(*sliceType, "Offset", "int32"));
    }
    if (indexExp)
    {
        elem& e = loadExp(irstate, *indexExp, &v);
        if (v)
        {
            //if the type is a slice, our back-end implements it as an ArraySegment,
            //which is a .NET valuetype ==> we need to discard the last LOAD instr.
            //that was generated, because a LOADA instr. will be generated further on
            if (toILType(*v).isSliceType())
            {
                assert(irstate.getBlock().back()->isLoad());
                irstate.getBlock().pop_back();
            }
        }
        else
        {   //if the index is not a variable then make a temporary
            //to store the index in case we need it later
            VarDeclaration* vd = new VarDeclaration(arrayVar.getDecl());
            vd->type = indexExp->type;            
            vd->ident = Lexer::idPool("idx_");

            if (e.isConst())
            {
                v = new VarAdapter(*vd, e);
                irstate.add(*v);
            }
            else
            {
                v = savePartialResult(irstate, *vd, indexExp->type);
                v->load(irstate.getBlock());
            }
        }
    }
    else if (indexVar)
    {
        indexVar->load(irstate.getBlock());
    }
    if (sliceType)
    {
        generateUpperBoundCheck(irstate, loc, *sliceType, DEREF(temp));
        Instruction::create(irstate.getBlock(), IL_add);
    }
    return v;
}


namespace {
    ///<summary>
    /// Helper that loads both the array variable and the index
    /// on the evaluation stack. See handling of IndexExp below.
    ///</summary>
    class ArrayIndex : public Variable
    {
        Variable* v1_;
        Variable* v2_;
        Variable& elem_;

        virtual elem* load(block& blk, Loc* loc = NULL, bool addr = false)
        {
            assert(!addr);
            DEREF(v1_).load(blk, loc);
            return DEREF(v2_).load(blk, loc);
        }
        virtual elem* store(block& blk, Loc* loc = NULL)
        { 
            elem_.store(blk, loc);
            return this;
        }
        elem* emitILAsm(wostream& out)
        {   // nothing to emit but a comment 
            indent(out) << "// " << v1_->getName() << '[' << v2_->getName() << "]\n";
            return NULL;
        }

    public:
        ArrayIndex(Variable& v, Variable* v1, Variable* v2)
            : Variable(v.getDecl()), v1_(v1), v2_(v2), elem_(v)
        { }
    };
}


// For class objects, identity is defined as the object references are for the same object.
// Null class objects can be compared with is.
// For struct objects, identity is defined as the bits in the struct being identical.
// For static and dynamic arrays, identity is defined as referring to the same array elements
// and the same number of elements.
// For other operand types, identity is defined as being the same as equality. 
elem* IRState::toElem(IdentityExp& exp)
{
    Type* lhsType = DEREF(exp.e1).type;
    elem* result;
    if (DEREF(lhsType).ty == Tclass)
    {
        loadExp(*this, DEREF(exp.e1));
        loadExp(*this, DEREF(exp.e2));
        result = Instruction::create(getBlock(), IL_ceq, &exp.loc);
        if (exp.op == TOKnotidentity)
        {
            result = LogicalNegation::create(getBlock(), &exp.loc);
        }
    }
    else
    {
        result = generateEqualOrIdentity<TOKidentity, TOKnotidentity, true>(*this, exp);
    }
    return result;
}


//An associative array can be tested to see if an element is in the array:
//if ("hello" in foo)
elem* IRState::toElem(InExp& exp)
{
    AssocArrayType& aaType = getAssocArrayType(exp.loc, DEREF(exp.e2).type);
    block& blk = getBlock();
    auto_ptr<block> loadPtr(new block(&blk, exp.loc, blk.depth()));
    auto_ptr<block> done(new block(&blk, exp.loc, blk.depth()));

    Identifier* id = Identifier::generateId("$tmp_");
    VarDeclaration* tmp = new VarDeclaration(exp.loc, &aaType.elemType(), id, NULL);
    Variable* v = NULL;
    {
        EmitInBlock group(*this, *blk.addNewBlock(&exp.loc));
        loadExp(*this, DEREF(exp.e2));
        Temporary<Encoding> setEncoding(encoding_, e_SystemString);
        loadExp(*this, DEREF(exp.e1), &v);
        AssocArrayElem::convertKey(getBlock(), aaType, v);
        Method& method = BackEnd::getCurrentMethod(exp.loc);

        //create temporary to store the value, if found
        v = method.addLocal(*tmp);

        //load address of temp, to pass to Dictionary(Tkey, Tval).TryGetValue(key, out val)
        v->load(getBlock(), &exp.loc, true);
        elem* tryGet = add(*new TryGetValue(aaType));

        //create conditional jump to loading a pointer to the value
        Branch::create(tryGet, getBlock(), loadPtr.get());

        Variable* null = method.addLocal(*tmp, tmp->type->pointerTo());
        null->load(getBlock());
        Branch::create(getBlock(), done.get());
    }
    v->load(*loadPtr, &exp.loc, true);
    blk.add(loadPtr);
    blk.add(done);

    v = savePartialResult(*this, *tmp, tmp->type->pointerTo());
    return v;
}


elem* IRState::toElem(IndexExp& exp)
{
    Variable* v = NULL;
    VarBlockScope varScope(*this, exp.loc);
    loadExp(*this, DEREF(exp.e1), &v);
    if (!v)
    {
         BackEnd::fatalError(exp.loc, "left-hand expression yielded null var");
    }
    // set the array variable so that if a $ symbol is encountered
    // we know which array it refers to
    Temporary<Variable*> setArrayContext(array_, v);

    TYPE* aType = &toILType(exp.loc, v->getType());
    if (PointerType* pt = aType->isPointerType())
    {
        if (v->hasClosureReference())
        {
            aType = &pt->getPointedType();
        }
    }
    Variable* keyVar = loadArrayIndex(*this, varScope, exp.loc, *v, *aType, exp.e2);
    Variable* res = NULL;
    if (aType->isArrayType())
    {
        res = ArrayElem::create(getBlock(), v->getDecl(), exp.type);
    }
    else if (AssocArrayType* aaType = aType->isAssociativeArrayType())
    {
        res = AssocArrayElem::create(getBlock(), *v, keyVar, aaType->elemType());
    }
    else
    {
        throw logic_error("array or assoc array expected");
    }
    // bind the element to the (array, index) pair, so that res->load()
    // works correctly when called more than once; the first time around
    // the array and the index are on the evaluation stack, but for subsequent
    // calls they need to be brought back on the stack (see loadRefParent)
    ArrayIndex* index = new ArrayIndex(DEREF(res), v, keyVar);
    getBlock().add(*index);
    res->bind(index);
    return res;
}


elem* IRState::toElem(IntegerExp& exp)
{
    TYPE& type = toILType(exp.loc, exp.type);
    elem* c = Const<int64_t>::create(type, exp.value, getBlock(), exp.loc);
    return init(*this, exp, c);
}


elem* IRState::toElem(MinExp& exp)
{
    return createBinaryOp(exp, IL_sub);
}


elem* IRState::toElem(MinAssignExp& exp)
{
    return createAssignOp(exp, IL_sub);
}


elem* IRState::toElem(ModAssignExp& exp)
{
    const Mnemonic op = IL_rem;
    return createAssignOp(exp, op);
}


elem* IRState::toElem(ModExp& exp)
{
    const Mnemonic op = IL_rem;
    return createBinaryOp(exp, op);
}


elem* IRState::toElem(MulExp& exp)
{
    return createBinaryOp(exp, IL_mul);
}


elem* IRState::toElem(MulAssignExp& exp)
{
    return createAssignOp(exp, IL_mul);
}


elem* IRState::toElem(NegExp& exp)
{
    loadExp(*this, DEREF(exp.e1));
    return Instruction::create(getBlock(), IL_neg, &exp.loc);
}


elem* IRState::toElem(NotExp& exp)
{
    loadExp(*this, DEREF(exp.e1));
    Type* t1 = DEREF(exp.e1).type;

    if ((DEREF(t1).ty != Tpointer) && DEREF(t1).isscalar())
    {
        Const<int>::create(*TYPE::Int32, 0, getBlock(), exp.loc);
    }
    else
    {
        add(*new Null);
    }
    return Instruction::create(getBlock(), IL_ceq, &exp.loc);
}


elem* IRState::toElemImpl(NewExp& exp)
{
    Symbol* sym = NULL;
    if (exp.member)
    {
        sym = exp.member->toSymbol();
    }
    else if (Type* t = exp.newtype)
    {
        IRState irstate(getBlock());
        ArrayAdapter<Expression*> args(exp.arguments);
        for (ArrayAdapter<Expression*>::iterator i = args.begin(); i != args.end(); ++i)
        {
            loadExp(irstate, DEREF(*i));
        }
        TYPE& newType = toILType(exp.loc, t);
        if (ArrayType* aType = newType.isArrayType())
        {
            return irstate.add(*NewArray::create(*aType));
        }
        else if (StructType* sType = newType.isStructType())
        {
            elem* ctor = new DefaultCtorCall<StructType>(getBlock(), IL_newobj, *sType);
            assert(ctor->getOwner());
            return ctor;
        }
        sym = t->toSymbol();
    }
    if (!sym)
    {
        BackEnd::fatalError(exp.loc, "callee is null");
    }
    if (Class* c = sym->isClass())
    {
        assert(c->getDecl().ctor == NULL);
        assert(c->getDecl().defaultCtor == NULL);
        elem* ctor = new DefaultCtorCall<ClassDeclaration>(getBlock(), IL_newobj, c->getDecl());
        assert(ctor->getOwner());
        return ctor;
    }
    Temporary<Method*> callScope(inCall_);
    //generate ctor call
    Method& method = prepCall(exp.loc, exp.arguments, sym);
    return CallInstruction::create(getBlock(), IL_newobj, method, &exp.loc);
}


elem* IRState::toElem(NewExp& exp)
{
    elem* e = toElemImpl(exp);

    if (!hasVarDecl())
    {
        Identifier* id = Lexer::idPool("new");
        VarDeclaration* decl = new VarDeclaration(exp.loc, exp.type, id, NULL);
        e = savePartialResult(*this, *decl, exp.type);
    }
    return e;
}


elem* IRState::toElem(OrAssignExp& exp)
{
    return createAssignOp(exp, IL_or);
}


elem* IRState::toElem(OrExp& exp)
{
    return createBinaryOp(exp, IL_or);
}


elem* IRState::toElem(OrOrExp& exp)
{
    return createLogicalOp(exp, IL_or);
}


elem* IRState::toElem(PostExp& exp)
{
    Variable* result = NULL;
    switch (exp.op)
    {
    case TOKplusplus:
        createAssignOp(exp, IL_add, &result);
        break;
    case TOKminusminus:
        createAssignOp(exp, IL_sub, &result);
        break;
    default:
        BackEnd::fatalError(exp.loc, "invalid opcode in post exp");
    }
    return result;
}


elem* IRState::toElem(PtrExp& exp)
{
    elem* e = &loadExp(*this, DEREF(exp.e1));
    if (Variable* v = e->isVariable())
    {
        //PtrExp-s are also generated by the front-end when
        //accessing struct fields
        if (toILType(exp.loc, v->getType()).isPointerType())
        {
            e = getBlock().add(*new Dereference(*v));
        }
        else
        {
            getBlock().pop_back();
        }
    }
    else if (e->isStringLiteral())
    {
        loadString(getBlock(), exp);
    }
    return e;
}


elem* IRState::toElem(NullExp& exp)
{
    return init(*this, exp, getBlock().add(*new Null));
}


static void setSliceTypeInfo(Declaration& decl, SliceExp& exp)
{
    ArrayType* const aType = toILType(exp.loc, decl.type).isArrayType();
    if (!aType)
    {
        BackEnd::fatalError(exp.loc, "non-array type in slice expression");
    }
    if (exp.lwr || exp.upr) //lower or upper bound specified?
    {
        SliceType* sliceType = BackEnd::instance().getSliceType(exp.loc, *aType);

        if (DEREF(decl.type).ctype != sliceType)
        {
            decl.type = sliceType->getType();
            assert(decl.type->ctype == sliceType);

            if (Symbol* sym = decl.csym)
            {   // attach the array slice type info to the declared var,
                // so that we can use lowerBound_ and upperBound_
                // in any further array element access via this slice
                if (Variable* v = sym->isVariable())
                {
                    v->setType(decl.type, exp.loc);
                }
            }
            exp.type = decl.type;
        }
    }
}


elem* IRState::toElem(ShlAssignExp& exp)
{
    return createAssignOp(exp, IL_shl);
}


elem* IRState::toElem(ShlExp& exp)
{
    return createBinaryOp(exp, IL_shl);
}


elem* IRState::toElem(ShrExp& exp)
{
    return createBinaryOp(exp, IL_shr);
}


elem* IRState::toElem(ShrAssignExp& exp)
{
    return createAssignOp(exp, IL_shr);
}


elem* IRState::toElem(UshrExp& exp)
{
    return createBinaryOp(exp, IL_shr_un);
}

elem* IRState::toElem(UshrAssignExp& exp)
{
    return createAssignOp(exp, IL_shr_un);
}


//When making a slice out of another slice, the offset needs to be adjusted
//relative to the first slice
static void adjustByOffset(IRState& irstate, SliceType* sliceType, Variable* v)
{
    if (sliceType && v)
    {
        v->load(irstate.getBlock(), NULL, true);
        irstate.add(*PropertyGetter::create(*sliceType, "Offset", "int32"));
        Instruction::create(irstate.getBlock(), IL_add);
    }
}


elem* IRState::toElem(SliceExp& exp)
{
    elem* result = NULL;
    SliceType* argSliceType = NULL;
    //are we in the context of an array declaration?
    if (VarDeclaration* decl = hasVarDecl())
    {
        IRState irstate(getBlock());
        setSliceTypeInfo(*decl, exp);
        result = &exprToElem(irstate, DEREF(exp.e1));
        Variable* v = result->isVariable();
        //the slice exp may occur on the left-handside
        //like this: int a[6] = ... in which case there
        //are no lwr and upr expressions

        // set the array variable so that if a $ symbol is encountered
        // we know which array it refers to
        Temporary<Variable*> setArrayContext(irstate.array_, v);
        if (v && (exp.lwr || exp.upr))
        {
            //put it in its own block so that removeArray can
            //deal with it atomically
            EmitInBlock group2(*this, *getBlock().addNewBlock());
            argSliceType = loadArray(irstate, *v, true);
        }
        if (exp.lwr)
        {
            assert(exp.upr);
            result = &exprToElem(irstate, *exp.lwr);
            adjustByOffset(irstate, argSliceType, v);
        }
        if (exp.upr)
        {
            assert(exp.lwr);
            result = &exprToElem(irstate, *exp.upr);
            adjustByOffset(irstate, argSliceType, v);
        }
    }
    else
    {
        ASlice* slice = new ASlice(&getBlock(), exp, getBlock().depth());
        getBlock().add(*slice);
        EmitInBlock group(*this, slice->getBlock());
        result = &exprToElem(*this, DEREF(exp.e1));
        Variable* v = result->isVariable();
        if (v)
        {
            EmitInBlock group2(*this, *getBlock().addNewBlock());
            argSliceType = loadArray(*this, *v, true);
        }

        // set the array variable so that if a $ symbol is encountered
        // we know which array it refers to
        Temporary<Variable*> setArrayContext(array_, v);
        if (exp.lwr)
        {
            assert(exp.upr);
            loadExp(*this, *exp.lwr);
            adjustByOffset(*this, argSliceType, v);
        }
        if (exp.upr)
        {
            assert(exp.lwr);
            loadExp(*this, *exp.upr);
            if (argSliceType)
            {
                adjustByOffset(*this, argSliceType, v);
                slice->setUpperAdjusted();
            }
        }
        slice->setElem(result);
        result = slice;
    }
    return result;
}


elem* IRState::toElem(StringExp& exp)
{
    StringLiteral* lit = new StringLiteral(exp, getBlock());
    if (encoding_ != e_Default)
    {
        lit->setEncoding(encoding_);
    }
    return init(*this, exp, lit);
}


elem* IRState::toElem(StructLiteralExp& exp)
{
    typedef ArrayAdapter<Expression*> ExprList;
    typedef ArrayAdapter<VarDeclaration*> VarDeclList;

    Variable* tmp = NULL;
    if (!hasVarDecl())
    {   // it the struct literal is not occurring in the initializer part
        // of a declaration, create a temporary variable to hold the struct
#if VALUETYPE_STRUCT
        Identifier* id = Identifier::generateId("$tmp_");
        VarDeclaration* vd = new VarDeclaration(exp.loc, exp.type, id, NULL);
        tmp = BackEnd::getCurrentMethod(exp.loc).addLocal(*vd);
#else
        assert(false); // todo
#endif
    }
    block* blk = getBlock().addNewBlock(&exp.loc);
    EmitInBlock group(*this, *blk);

    // Iterate through both the struct's fields and expression list,
    // and initialize fields from expressions
    VarDeclList varDecls(DEREF(exp.sd).fields);
    VarDeclList::iterator f = varDecls.begin();
    ExprList elems(exp.elements);
    for (ExprList::iterator i = elems.begin(), end = elems.end();
         (i != end) && (f != varDecls.end());
         ++i, ++f)
    {
        Expression* e = *i;
        if (!e)
        {
            continue; // may have NULL entries for fields to skip
        }

        // For struct literals on the right hand-side of a declaration,
        // assume that the address of the valuetype is on top of the stack,
        // duplicate it (unless last in elems list), to leave one for the
        // next element.
        // Not needed in assignments such as: s = S();
        if ((i + 1 != end) && hasVarDecl())
        {
            Instruction::create(*blk, IL_dup);
        }

        Variable* v = NULL;
        loadExp(*this, *e, &v);
        if (Symbol* sym = DEREF(f)->csym)
        {
            if (v)
            {
                blk->swapTail();
            }
            v = sym->isVariable();
            if (v)
            {
                v->unbind(true); // so that foreach opApply works correctly
                if (tmp)
                {
                    tmp->load(getBlock(), &exp.loc, true);
                }
                v->store(*blk);
            }
            else
            {
                BackEnd::fatalError(exp.loc, "struct field has no back-end field");
            }
        }
        else
        {
            BackEnd::fatalError(exp.loc, "struct field has no associated symbol");
        }
    }
    if (tmp)
    {
        return tmp;
    }
    return blk;
}



//The front-end generates SymOffExp's when fields of structs
//or elements of arrays are accessed.
elem* IRState::toElem(SymOffExp& offExp, Variable& v)
{
    elem* e = &v;
    if (DEREF(v.getType()).ty == Tstruct)
    {
        //get struct field by offset
        TypeStruct* tstruct = static_cast<TypeStruct*>(v.getType());
        if (Struct* s = DEREF(tstruct->sym->csym).isStruct())
        {
            //assume that the address of field 0 is the
            //same with the address of the struct
            if (offExp.offset == 0)
            {
                // this also works for empty structs
                e = v.load(getBlock(), &offExp.loc, true);
            }
            else
            {
                e = s->getField(offExp.offset);
                if (!e)
                {
                    error(offExp.loc, "field [%d] not found", offExp.offset); 
                }
                else if (Symbol* sym = e->isSymbol())
                {
                    sym->bind(&v);
                    e = sym;
                }
            }
        }
    }
    // get array element's address
    else if (ArrayType* aType = toILType(offExp.loc, v.getType()).isArrayType())
    {
        v.load(getBlock());
        assert(aType->elemType().size());
        size_t index = offExp.offset / aType->elemType().size();
        Const<size_t>::create(*TYPE::Uint, index, getBlock(), offExp.loc);
        e = ArrayElem::create(getBlock(), v.getDecl(), &aType->elemType());
    }
    else
    {
        assert(offExp.offset == 0);
        // do not load here since it breaks some use cases, let callers do it
        // e = v.load(getBlock(), &offExp.loc, true /* load address */);
    }
    return e;
}


static Field* addToClosure(IRState&, Method&, Variable&, Loc&);


static Field* addToClosure(IRState& irstate, 
                           Closure& closure,
                           Aggregate& aggr,
                           Method& method,
                           Variable& v,
                           Loc& loc
                           )
{
    //if the variable's lexical scope is higher than this
    //function's parent, and the parent is also a nested function,
    //add it to the parent's closure
    if (v.getDecl().parent != method.funcDecl().parent)
    {
        if (Method* parent = method.parent())
        {
            if (parent->isNested())
            {
                return addToClosure(irstate, *parent, v, loc);
            }
        }
    }
    //clone the variable's declaration
    VarDeclaration* decl = new VarDeclaration(v.getDecl());
    decl->parent = &aggr.getDecl();
    decl->loc = loc;
    decl->ident = Identifier::generateId(decl->toChars());

    if (v.hasClosureReference())
    {
        Type* t = DEREF(decl->type).pointerTo();
        if (PointerType* pt = toILType(loc, t).isPointerType())
        {
            t->ctype = pt->getUnmanagedType();
        }
        decl->type = t;
        aggr.getDecl().storage_class |= STCpinned;
    }
    Field* f = Field::create(*decl, false);
    f->bind(getThisArg(method.funcDecl()));
    irstate.add(*new ThisArg);
    // add the field directly to the back-end block
    aggr.getBlock().add(*f);
    closure.addMapping(v, f);
    return f;
}


static Field* addToClosure(IRState& irstate, Method& m, Variable& v, Loc& loc)
{
    Closure* closure = m.getClosure(true);
    Field* f = closure->isMapped(v);

    if (!f)
    {
        //the variable is not mapped to a field in the closure,
        //create the field and the mapping
        if (DEREF(v.getType()).ty == Tpointer)
        {
            error(loc, "%s cannot access pointers in closure", BackEnd::name());
        }
        else if (Aggregate* aggr = m.getAggregate())
        {
            f = addToClosure(irstate, *closure, *aggr, m, v, loc);
        }
        else
        {
            m.setClosure(NULL);
        }
    }
    return f;
}


// Detect and handle the case when a nested function accesses a variable
// in the "parent" activation record.
static Variable* handleNestedAccess(IRState& irstate,
                                    FuncDeclaration* fdecl,
                                    SymbolExp& exp,
                                    Variable& v
                                    )
{
    assert(v.isLocalOrParam());
    Variable* result = &v;

    Method& m = toMethod(*fdecl);
    if (v.getOwner() != &m.getLocals()  &&
        v.getOwner() != &m.getArgs()    &&
        v.getOwner() != m.getVarScope()
        )
    {
        if (Field* f = addToClosure(irstate, m, v, exp.loc))
        {
            result = f;
        }
    }
    return result;
}


//The $ symbol means the length of the array (as in a[5..$])
elem* IRState::handleDollarSymbol(SymbolExp& exp)
{
    Declaration& decl = DEREF(exp.var);
    if (decl.ident == Id::dollar) // handle special $ symbol
    {
        if (array_)
        {
            array_->load(getBlock(), &exp.loc);
            Variable* len = new ArrayLen(*array_, array_->getType());
            add(*len);
            if (SliceType* st = toILType(exp.loc, array_).isSliceType())
            {
                len->load(getBlock());
                return getBlock().back();
            }
            else
            {
                return len;
            }
        }
        else
        {
            BackEnd::fatalError(exp.loc, "NULL array context for $ symbol");
        }
    }
    return NULL;
}


static Variable* checkNestedAccess(IRState& irstate,
                                   SymbolExp& exp,
                                   Variable& v,
                                   VarDeclaration& vdecl
                                   )
{
    typedef ArrayAdapter<FuncDeclaration*> NestedFuncs;

    //nestedrefs is an array of all nested functions that refer the variable   
    if (vdecl.nestedrefs.dim)
    {        
        ModuleEmitter& modemit = BackEnd::getModuleScope(exp.loc);
        if (FuncDeclaration* fdecl = modemit.getCurrentFuncDeclaration())
        {        
            NestedFuncs nestedRefs(vdecl.nestedrefs);
            if (FuncDeclaration* fnested = nestedRefs.find(fdecl))
            {
                return handleNestedAccess(irstate, fnested, exp, v);
            }
        }
    }
    return &v;
}


elem* IRState::toElem(SymbolExp& exp)
{
    elem* e = handleDollarSymbol(exp);
    if (e)
    {
        return e;
    }
    e = exp.var->toSymbol();
    if (!e)
    {
        IF_DEBUG(clog << exp.toChars() << ": toSymbol() returned null\n");
    }
    else if (Variable* v = e->isVariable())
    {
        if (v->isLocalOrParam())
        {
            e = v = checkNestedAccess(*this, exp, *v, v->getDecl());
        }
        if (exp.op == TOKsymoff)
        {
            e = toElem(static_cast<SymOffExp&>(exp), *v);
        }
    }
    else if (exp.op == TOKsymoff)
    {
        //wrap pointer to func into .NET delegate 
        if (Method* method = e->isMethod())
        {
            add(*new Null);
            LoadFunc::create(getBlock(), *method, &exp.loc);
            e = add(*new Delegate(toILType(exp.loc, exp.type)));
        }
        else
        {
            BackEnd::fatalError(exp.loc, "null var in symbol offset expression");
        }
    }
    else if (Aggregate* aggr = e->isAggregate())
    {
        //initializer expression expected by init()
        Identifier* ident = Identifier::generateId("$INI_");
        VarDeclaration* vd = new VarDeclaration(exp.loc, aggr->getDecl().type, ident, NULL);
        vd->ident = ident;

        Variable* tmp = BackEnd::getCurrentMethod(exp.loc).addLocal(*vd);
        tmp->load(getBlock());

        e = init(*this, exp, e);
    }
    return e;
}


elem* IRState::toElem(RealExp& exp)
{
    TYPE& type = toILType(exp.loc, exp.type);
    return init(*this, exp, Const<double>::create(type, exp.value, getBlock(), exp.loc));
}


elem* IRState::toElem(RemoveExp& exp)
{
    AssocArrayType& aaType = getAssocArrayType(exp.loc, DEREF(exp.e1).type);
    loadExp(*this, DEREF(exp.e1));
    Temporary<Encoding> setEncoding(encoding_, e_SystemString);
    Variable* key = NULL;
    loadExp(*this, DEREF(exp.e2), &key);
    AssocArrayElem::convertKey(getBlock(), aaType, key);
    return add(*new Remove(aaType));
}


elem* IRState::toElem(ThisExp& exp)
{
    if (Declaration* dsym = exp.var)
    {
        return dsym->toSymbol();
    }
    return getBlock().add(*new ThisArg);
}


elem* IRState::toElem(TypeExp& exp)
{
    error(exp.loc, "type %s is not an expression", exp.toChars());
    return add(*new Null);
}


elem* IRState::toElem(XorAssignExp& exp)
{
    return createAssignOp(exp, IL_xor);
}


elem* IRState::toElem(XorExp& exp)
{
    return createBinaryOp(exp, IL_xor);
}


//
// place-holders, to be removed as the real thing gets implemented...
//
#define TO_ELEM_IMPL(E) elem* IRState::toElem(E&) { NOT_IMPLEMENTED(NULL); }

TO_ELEM_IMPL(ComExp)
TO_ELEM_IMPL(ComplexExp)
TO_ELEM_IMPL(Expression)
TO_ELEM_IMPL(HaltExp)
TO_ELEM_IMPL(ScopeExp)
TO_ELEM_IMPL(TupleExp)


#pragma endregion

/*****************************************************************
 * helpers for labeled break and continue statements
 */
static inline block* findBlock(IRState::LabeledBlocks& blocks,
                               Identifier* ident, 
                               block* defaultBlock
                               )
{
    IRState::LabeledBlocks::const_iterator i = blocks.find(ident);
    if (i == blocks.end())
    {
        return defaultBlock;
    }
    else
    {
        return i->second;
    }
}


static void checkLabel(const block& owner,
                       block* blk,
                       IRState::LabeledBlocks& blocks
                       )
{
    if (Label* label = owner.hasLabel())
    {
        if (Identifier* ident = label->getIdent())
        {
            blocks.insert(make_pair(ident, blk));
        }
    }
}


void IRState::setLoopContext(LoopContext& ctxt,
                             block* brk,
                             block* cont,
                             block* current
                             )
{
    ctxt.break_ = break_;
    ctxt.cont_ = cont_;
    break_ = brk;
    cont_ = cont;
    if (!current)
    {
        current = &getBlock();
    }
    checkLabel(*current, break_, labeledBreak_);
    checkLabel(*current, cont_, labeledCont_);
}


/****************************************/
#pragma region toIR


void IRState::toIR(AsmStatement& stat)
{
    error(stat.loc, "asm statements are not supported by %s", BackEnd::name());
}


void IRState::toIR(BreakStatement& stat)
{
    if (block* loopEnd = findBlock(labeledBreak_, stat.ident, break_))
    {
        // Do not need to explicitly put the branch into a block;
        // this should occur naturally, because we expect the "break"
        // statement to be inside of a conditional block.

        //block* blk = getBlock().addNewBlock(stat.loc);
        //Branch::create(*blk, loopEnd);

        Branch::create(getBlock(), loopEnd);
    }
}


void IRState::toIR(ContinueStatement& stat)
{
    if (block* loopCont = findBlock(labeledCont_, stat.ident, cont_))
    {
        // Do not need to explicitly put the branch into a block;
        // this should occur naturally, because we expect the "continue"
        // statement to be inside of a conditional block within the loop
        //block* blk = getBlock().addNewBlock(stat.loc);
        //Branch::create(*blk, loopCont);

        Branch::create(getBlock(), loopCont);
    }
    else
    {
        BackEnd::fatalError(stat.loc, "not in the context of a loop?");
    }
}


void IRState::toIR(DoStatement& stat)
{
    genWhileLoop(DEREF(stat.body), DEREF(stat.condition), false);
}


void IRState::toIR(ExpStatement& stat)
{
    if (Expression* exp = stat.exp)
    {
        elem* e = exp->toElem(this);
        if (e && e->isLoadFunc())
        {
            //add(*new Delegate(toILType(stat.loc, exp->type)));
            getBlock().pop_back();
            getBlock().pop_back();
        }
    }
}



///<summary>
/// Generate FOR statement
///</summary>
void IRState::toIR(ForStatement& stat)
{
    block& owner = getBlock();
    const unsigned depth = owner.depth();

    //Enclose the entire "for" loop code inside a block,
    //to manage the scope of variables local to the loop.
    VarBlockScope forScope(*this, stat.loc);

    if (stat.init)
    {
        stat.init->toIR(this);
    }
    auto_ptr<block> cond(new block(&getBlock(), stat.loc, depth + 1));
    auto_ptr<block> done(new block(&getBlock(), stat.loc, depth));

    block* jumpToTest = getBlock().addNewBlock(stat.loc);

    {   EmitInBlock emitInBlock(*this, *jumpToTest);
        // unconditionally go to testing condition
        Branch::create(getBlock(), cond.get());
    }
    block* body = getBlock().addNewBlock(stat.loc);
    block* incr = getBlock().addNewBlock(stat.loc);

    LoopContext ctxt;
    setLoopContext(ctxt, done.get(), incr, &owner);

    // Generate loop body
    {   EmitInBlock emitInBlock(*this, *body);
        if (stat.body)
        {
            stat.body->toIR(this);
        }
    }
    // Generate increment
    {
        EmitInBlock emitInBlock(*this, *incr);
        if (stat.increment)
        {
            exprToElem(*this, *stat.increment);
        }
    }
    // Generate code that tests the condition
    // for staying in the loop
    {   EmitInBlock emitInBlock(*this, *cond);
        if (stat.condition)
        {
            elem* c = &loadExp(*this, *stat.condition);
            // conditional branch to body
            Branch::create(c, getBlock(), body);
            // otherwise fall to the end
        }
        else
        {   // empty condition is always true:
            // jump unconditionally to the body
            Branch::create(getBlock(), body);
        }
    }
    getBlock().add(cond);
    // "break" statement encountered?
    if (done->isReferenced())
    {
        getBlock().add(done);
    }
}


//Load the size of an array onto the evaluation stack;
//if the array is static, the size is known at compile time (const)
static void loadArraySize(block& blk,
                          Loc& loc,
                          Variable& arrayVar,
                          ArrayType& arrayType
                          )
{
    if (Expression* exp = arrayType.getSize())
    {
        Const<int>::create(*TYPE::Int32, exp->toInteger(), blk, blk.loc());
    }
    else
    {
        auto_ptr<block> doneBlk(new block(&blk, loc, blk.depth()));
        auto_ptr<block> okayBlk(new block(&blk, loc, blk.depth()));

        if (!arrayType.isSliceType())
        {
            block* testNullBlk = blk.addNewBlock(&loc);
            elem* cond = arrayVar.load(*testNullBlk);
            //check for null array object:
            Branch::create(cond, *testNullBlk, okayBlk.get());
            // if null, the flow control falls thru here:
            Const<int>::create(*TYPE::Int32, 0, *testNullBlk, loc);
            //create non-conditional branch to doneBlk:
            Branch::create(*testNullBlk, doneBlk.get());
        }

        //if array is not null, proceed with loading the length
        arrayVar.load(*okayBlk);
        ArrayLen* len = new ArrayLen(arrayVar, arrayVar.getType());
        okayBlk->add(*len);
        len->load(*okayBlk, &loc);

        blk.add(okayBlk);
        blk.add(doneBlk);
    }
}



// Generate foreach code for non-associative arrays
void IRState::foreachArrayElem(ForeachStatement& stat,
                               ArrayType& aType,
                               Variable& aggr,
                               Variable& key,
                               Variable* value
                               )
{
    Temporary<Expression*> saveAggr(foreachAggr_);
    foreachAggr_ = stat.aggr;
   
    //begin inner scope for loop variables
    VarBlockScope varScope(*this, stat.loc);

    const unsigned depth = getBlock().depth();
    //create block for evaluating condition for staying in the loop
    auto_ptr<block> cond(new block(&getBlock(), stat.loc, depth));
    //create block where to break out of the loop
    auto_ptr<block> done(new block(&getBlock(), stat.loc, depth));

    block* jumpToTest = getBlock().addNewBlock(stat.loc);
    block* body = getBlock().addNewBlock(stat.loc);
    block* incr = getBlock().addNewBlock(stat.loc);

    LoopContext ctxt;
    setLoopContext(ctxt, done.get(), incr);

    {   EmitInBlock emitInBlock(*this, *jumpToTest);
        if (stat.op == TOKforeach_reverse)
        {
            loadArraySize(getBlock(), stat.loc, aggr, aType);
            key.store(getBlock());
            Branch::create(getBlock(), incr);
        }
        else
        {
            Const<int>::create(*TYPE::Int32, 0, getBlock(), stat.loc);
            key.store(getBlock());
            Branch::create(getBlock(), cond.get());
        }
    }
    // emit body
    {
        EmitInBlock emitInBlock(*this, *body);
        if (value)
        {
            aggr.load(getBlock());
            loadArrayIndex(*this, varScope, stat.loc, aggr, aType, NULL, &key);
            ArrayElemAccess::load(getBlock(), *value);
            value->store(getBlock());

            //work around the front-end not calling the copy constructor
            //when foreach iterates by value over an array of structs
            if (!stat.value->isRef() && (stat.value->type->ty == Tstruct))
            {
                TypeStruct* ts = static_cast<TypeStruct*>(stat.value->type);
                if (FuncDeclaration* fd = DEREF(ts->sym).postblit)
                {
                    Method& method = getMethod(stat.loc, *fd);
                    value->load(getBlock(), &stat.loc, true);                    
                    CallInstruction::create(getBlock(), IL_call, method, &stat.loc);
                }
            }
        }

        //todo optimization: generate this only if value is assigned to
        if (value->getDecl().isRef())
        {
            VarDeclaration& vd = value->getDecl();
            {
                Temporary<unsigned> storage(vd.storage_class);
                vd.storage_class &= ~(STCref | STCforeach);
                DEREF(stat.body).toIR(this);
            }
            aggr.load(getBlock());
            loadArrayIndex(*this, varScope, stat.loc, aggr, aType, NULL, &key);
            value->load(getBlock());
            ArrayElemAccess::store(getBlock(), aType.elemTYPE());
        }
        else
        {
            verifyForeachRef(stat.loc, value->getDecl(), foreachAggr());
            DEREF(stat.body).toIR(this);
        }
    }
    //increment the key
    {
        EmitInBlock emitInBlock(*this, *incr);
        key.load(getBlock());
        Const<int>::create(*TYPE::Int32, 1, getBlock(), stat.loc);
        switch (stat.op)
        {
        case TOKforeach:
            Instruction::create(getBlock(), IL_add);
            break;
        case TOKforeach_reverse:
            Instruction::create(getBlock(), IL_sub);
            break;
        default:
            BackEnd::fatalError(stat.loc, "unhandled foreach op");
        }
        key.store(getBlock());
    }
    // generate loop end condition, which depends on the sense of the traversal
    {   EmitInBlock emitInBlock(*this, *cond);
        key.load(getBlock());
        Mnemonic op = IL_nop;
        switch (stat.op)
        {
        case TOKforeach:
            loadArraySize(getBlock(), stat.loc, aggr, aType);
            //continue if key is less than array size
            op = IL_blt;
            break;

        case TOKforeach_reverse:
            Const<int>::create(*TYPE::Int32, 0, getBlock(), stat.loc);
            //continue if key is greater or equal than zero
            op = IL_bge;
            break;

        default:
            BackEnd::fatalError(stat.loc, "unhandled foreach op");
        }
        elem* br = BranchInstruction::create(getBlock(), op);
        Branch::create(br, getBlock(), body);
    }
    getBlock().add(cond);
    if (done->isReferenced()) // "break" statement encountered?
    {
        getBlock().add(done);
    }
}


void IRState::toIR(ForeachRangeStatement& stat)
{
    if (stat.body && stat.key)
    {
        // variables are in the scope of the foreach loop
        VarBlockScope foreachScope(*this, stat.loc);
        Method& method = foreachScope.getMethod();
        Variable* key = method.addLocal(*stat.key);
        stat.key->csym = key;
        SymbolExp* sym(new SymbolExp(stat.loc, TOKvar, sizeof(VarExp), stat.key, 0));
        sym->type = stat.key->type;

        const unsigned depth = getBlock().depth();
        block* cond = new block(&getBlock(), stat.loc, depth);
        block* done = new block(&getBlock(), stat.loc, depth);

        LoopContext ctxt;
        setLoopContext(ctxt, done, cond);
 
        //foreach starts at lwr, foreach_reverse starts at upr - 1
        Expression* begin = stat.lwr;
        Expression* end = stat.upr;
        int step = 1;
        Mnemonic condOp = IL_bge;
        if (stat.op == TOKforeach_reverse)
        {
            end = stat.lwr;
            begin = stat.upr;
            step = -1;
            condOp = IL_ble;
        }
        Expression* inc = new IntegerExp(stat.loc, step, sym->type);
        AssignExp* assign = new AssignExp(stat.loc, sym, begin);
        assign->type = sym->type;
        assign->toElem(this);

        Expression* next = new AddAssignExp(stat.loc, sym, inc);
        next->type = sym->type;

        //generate the condition for staying in the loop
        {   EmitInBlock loopCondition(*this, *cond);
            key->load(*cond);
            loadExp(*this, DEREF(end));
            elem* br = BranchInstruction::create(getBlock(), condOp);
            Branch::create(br, getBlock(), done);
        }
        add(*cond);
        
        if (stat.op == TOKforeach_reverse)
        {
            exprToElem(*this, *next);
            stat.body->toIR(this); //generate loop body
        }
        else
        {
            stat.body->toIR(this); //generate loop body
            exprToElem(*this, *next);
        }
        Branch::create(getBlock(), cond);
        add(*done);
    }
}


void IRState::toIR(ForeachStatement& stat)
{
    if (stat.body)
    {
        VarBlockScope varScope(*this, stat.loc);

        elem& e = exprToElem(*this, DEREF(stat.aggr));
        Variable* aggr = e.isVariable();

        if (aggr && aggr->isBound())
        {
            aggr->unbind();
            // remove the owner object from the eval stack since
            // the loading of aggr is delayed until later
            getBlock().pop_back();            
        }
        Method& method = varScope.getMethod();

        assert(stat.func);
        assert(stat.func->csym);

        Variable* key = NULL;
        Variable* value = NULL;
        if (stat.key)
        {
            stat.key->csym = key = method.addLocal(*stat.key);
        }
        else
        {
            Identifier* ident = Identifier::generateId("key");
            VarDeclaration* vdecl = new VarDeclaration(stat.loc, Type::tint32, ident, NULL);
            key = method.addLocal(*vdecl);
            vdecl->csym = key;
        }
        if (stat.value)
        {
            stat.value->csym = value = method.addLocal(*stat.value);
        }
        Type* type = aggr ? aggr->getType() : stat.aggr->type;
        if (ArrayType* aType = toILType(stat.loc, type).isArrayType())
        {
            if (!aggr)
            {
                if (e.isSliceFiller())
                {
                    e.getOwner()->pop_back(); //slice filler
                }
                Identifier* ident = Identifier::generateId("tmp");
                VarDeclaration* vdecl = new VarDeclaration(stat.loc, type, ident, NULL);
                aggr = method.addLocal(*vdecl);
                vdecl->csym = aggr;
                aggr->store(getBlock());
            }
            foreachArrayElem(stat, *aType, *aggr, *key, value);
        }
        else
        {
            NOT_IMPLEMENTED();
        }
    }
}


void IRState::toIR(HiddenMethodStatement& stat)
{
    //generate code for throwing a HiddenFuncError exception
    class HiddenFuncError : public elem
    {
        Method& method_; //the hidden method

        int32_t getMaxStackDelta() const
        {
            return 1;
        }
        elem* emitILAsm(wostream& out)
        {
            indent(out) << IL_ldstr << " \"" << method_.getProto() << "\"\n";
            indent(out) << "newobj instance void class [dnetlib]core.HiddenFuncError::.ctor(string)\n";
            indent(out) << IL_throw << "\n";
            return this;
        }

    public:
        HiddenFuncError(HiddenMethodStatement& stat) : method_(stat.getMethod())
        {
        }
    }; // HiddenFuncError

    add(*new HiddenFuncError(stat));
}


void IRState::toIR(UnrolledLoopStatement& stat)
{
    ArrayAdapter<Statement*> stats(stat.statements);
    for (ArrayAdapter<Statement*>::iterator i = stats.begin(); i != stats.end(); ++i)
    {
        (*i)->toIR(this);
    }
}


///<summary>
/// Generate while statement
///</summary>
void IRState::toIR(WhileStatement& stat)
{
    genWhileLoop(DEREF(stat.body), DEREF(stat.condition), true);
}


namespace {
    ///<summary>
    ///Emit System.Threading.Monitor.Enter / Exit calls,
    ///used in generating code for SynchronizedStatement
    ///</summary>
    class Monitor : public elem
    {
    public:
        enum Operation { m_ENTER, m_EXIT };
        explicit Monitor(Operation op) : op_(op) { }

    private:
        // both Enter and Exit pop the Object argument, return void
        int32_t getStackDelta() const { return -1; } 

        elem* emitILAsm(wostream& out)
        {
            indent(out) << "call void class [mscorlib]System.Threading.Monitor::";
            switch (op_)
            {
            case m_ENTER: out << "Enter(object)\n";
                break;

            case m_EXIT: out << "Exit(object)\n";
                break;
            }
            return this;
        }

    private:
        Operation op_;
    };
}


void IRState::toIR(SynchronizedStatement& stat)
{
    Variable* v = NULL;
    if (stat.exp)
    {
        VarBlockScope varScope(*this, stat.exp->loc);
        loadExp(*this, *stat.exp, &v);
        if (v)
        {
            v = varScope.getMethod().addLocal(v->getDecl(), v->getType());
            v->store(getBlock());
        }
    }
    else
    {   // http://www.digitalmars.com/d/2.0/statement.html#SynchronizedStatement
        // "If there is no Expression, then a global mutex is created, one per such 
        // synchronized statement. Different synchronized statements will have
        // different global mutexes."
        Type* objType = DEREF(ClassDeclaration::object).type;
        Identifier* ident = Identifier::generateId("$mutex_");
        NewExp* newExp = new NewExp(stat.loc, NULL, NULL, objType, NULL);
        Initializer* init = new ExpInitializer(stat.loc, newExp);
        Scope* sc = new Scope();
        init->semantic(sc, objType);
        VarDeclaration* mutex = new VarDeclaration(stat.loc, objType, ident, init);
        v = BackEnd::getModuleScope(stat.loc).createGlobalVar(*mutex);
    }

    if (stat.body)
    {
        const unsigned depth = getBlock().depth();
        block* blk = new block(&getBlock(), stat.loc, depth + 1);
        getBlock().add(*blk);
        TryCatchHandler* eh = new TryCatchHandler(blk, stat, depth);
        blk->setEH(eh);
        /********* emit try block *********/
        {   EmitInBlock blockGuard(*this, *blk);
            if (v)
            {
                v->load(*blk, &stat.loc);
                blk->add(*new Monitor(Monitor::m_ENTER));
            }
            stat.body->toIR(this);
        }
        /********* emit final block *********/
        if (block* final = eh->genFinally(*this, NULL))
        {
            if (v)
            {
                v->load(*final, &stat.loc);
                final->add(*new Monitor(Monitor::m_EXIT));
            }
        }
    }
}


//Handle variable labels: emit an if-else-... chain
void IRState::handleVarCases(SwitchStatement& stat, auto_ptr<block> defaultBlock)
{
    block* blk = getBlock().addNewBlock(stat.loc);
    EmitInBlock emitInBlock(*this, *blk);

    EqualExp* condition = new EqualExp(TOKequal, stat.loc, stat.condition, NULL);

    ArrayAdapter<CaseStatement*> cases(stat.cases);
    for (ArrayAdapter<CaseStatement*>::const_iterator i = cases.begin();
         i != cases.end();
         ++i)
    {
        condition->e2 = (*i)->exp;
        elem* cond = &loadExp(*this, *condition);
        Branch::create(cond, getBlock(), (*i)->cblock);
    }   
    fallThroughDefault(defaultBlock);
}


void IRState::handleConstCases(SwitchStatement& stat,
                               // block in which condition is evaluated
                               block& evalBlock,
                               auto_ptr<block> defaultBlock
                               )
{
    typedef std::map<int64_t, Label*> CaseIndexToLabelMap;
    CaseIndexToLabelMap caseIndexToLabelMap;

    ArrayAdapter<CaseStatement*> cases(stat.cases);
    for (ArrayAdapter<CaseStatement*>::const_iterator i = cases.begin();
         i != cases.end();
         ++i
         )
    {
        Expression& exp = DEREF((*i)->exp);
        if ((i == cases.begin()) && DEREF(exp.type).isString())
        {
            handleVarCases(stat, defaultBlock);
            return;
        }
        Label& label = DEREF((*i)->cblock).getLabel(true);
        const int caseIndex = DEREF((*i)->exp).toInteger();
        caseIndexToLabelMap.insert(make_pair(caseIndex, &label));
    }

    //generate IL switch instruction(s)
    {   EmitInBlock emitInBlock(*this, evalBlock);
        if (!caseIndexToLabelMap.empty())
        {
            CaseIndexToLabelMap::const_iterator it = caseIndexToLabelMap.begin();
            CaseIndexToLabelMap::const_iterator end = caseIndexToLabelMap.end();

            int64_t previousCaseOffset = (*it).first;
            int64_t minCaseOffset = (*it).first;

            // collect case labels, to be passed to SwitchInstruction::create
            vector<Label*> labels;
            MinExp* minExp(new MinExp(stat.loc, stat.condition, NULL));

            for (; it != end; ++it)
            {
                if ((*it).first - previousCaseOffset > 1)
                {
                    // computing jump offset
                    minExp->e2 = new IntegerExp(minCaseOffset);
                    exprToElem(*this, *minExp);

                    // emit a new IL switch statement for the contiguous region.
                    SwitchInstruction::create(getBlock(), labels, IL_switch, &stat.loc);

                    // reset stats for the next region
                    labels.clear();
                    minCaseOffset = (*it).first;
                }
                labels.push_back((*it).second);
                previousCaseOffset = (*it).first;
            }

            minExp->e2 = new IntegerExp(minCaseOffset);
            exprToElem(*this, *minExp);
            SwitchInstruction::create(getBlock(), labels, IL_switch, &stat.loc);
        }
        fallThroughDefault(defaultBlock);
    }
}


void IRState::toIR(GotoCaseStatement& stat)
{
    if (stat.cs)
    {
        Branch::create(getBlock(), stat.cs->cblock);
    }
}


void IRState::toIR(GotoDefaultStatement& /* stat */)
{
    if (default_)
    {
        Branch::create(getBlock(), default_);
    }
}


void IRState::toIR(SwitchErrorStatement& stat)
{
    static const char* err = "[dnetlib]core.SwitchError";
    new DefaultCtorCall<const char*>(getBlock(), IL_newobj, err);
    Instruction::create(getBlock(), IL_throw, &stat.loc);
}


///<summary>
/// Generate code for switch statement
///</summary>
void IRState::toIR(SwitchStatement& stat)
{
    const unsigned depth = getBlock().depth();

    auto_ptr<block> cond(new block(&getBlock(), stat.loc, depth));
    auto_ptr<block> end(new block(&getBlock(), stat.loc, depth));
    auto_ptr<block> body(new block(&getBlock(), stat.loc, depth));
    auto_ptr<block> sdefault;
    
    //emit the cases first
    {   EmitInBlock emitInBlock(*this, *body);
        sdefault.reset(new block(&getBlock(), stat.loc, depth));
        Temporary<block*> setBreak(break_, end.get());
        Temporary<block*> setDefault(default_, sdefault.get());
        DEREF(stat.body).toIR(this);
    }
    //emit the flow control for selecting the case
    if (stat.hasVars || DEREF(stat.cases).dim == 1)
    {
        handleVarCases(stat, sdefault);
    }
    else
    {
        handleConstCases(stat, *cond, sdefault);
    }
    getBlock().add(cond);
    getBlock().add(body);
    getBlock().add(end);
}


///<summary>
/// generate unconditional branch to default switch statement
///</summary>
void IRState::fallThroughDefault(auto_ptr<block> sdefault)
{
    if (sdefault->getOwner())
    {
        Branch::create(getBlock(), sdefault.release());
    }
}


///<summary>
/// Generate code for switch case statement
///</summary>
void IRState::toIR(CaseStatement& stat)
{
    if (Statement* s = stat.statement)
    {
        stat.cblock = getBlock().addNewBlock(&stat.loc);
        EmitInBlock emitInBlock(*this, *stat.cblock);

        s->toIR(this);
    }
}


///<summary>
/// Generate code for default switch case statement
///</summary>
void IRState::toIR(DefaultStatement& stat)
{
    if (Statement* s = stat.statement)
    {
        if (default_)
        {
            add(*default_);
            EmitInBlock emitInBlock(*this, *default_);
            s->toIR(this);
        }
    }
}


///<summary>
/// Generate code for if-else statement
///</summary>
void IRState::toIR(IfStatement& stat)
{
    const unsigned depth = getBlock().depth();
    auto_ptr<block> ifBlock(new block(&getBlock(), DEREF(stat.ifbody).loc, depth));

    // a block for where the branches merge back together
    auto_ptr<block> joinBlock(new block(&getBlock(), stat.loc, depth));
    {
        block* blk = getBlock().addNewBlock(stat.loc);
        EmitInBlock emitInBlock(*this, *blk);

        // emit condition
        elem* cond = &loadExp(*this, DEREF(stat.condition));
        // conditional branch to if block
        Branch::create(cond, getBlock(), ifBlock.get());

        // generate IF block
        EmitInBlock(*this, DEREF(stat.ifbody), *ifBlock);

        if (stat.elsebody)
        {
            auto_ptr<block> elseBlock(new block(&getBlock(), stat.elsebody->loc, depth));
            elseBlock->setParallelCF(*ifBlock);
            EmitInBlock(*this, *stat.elsebody, *elseBlock, joinBlock.get());
            getBlock().add(elseBlock);
        }
        // unconditionally jump to where the branches join back together
        // (i.e. jump over the if block)
        Branch::create(getBlock(), joinBlock.get());
    }
    getBlock().add(ifBlock);
    getBlock().add(joinBlock);
}


void IRState::toIR(LabelStatement& stat)
{
    block* blk = getBlock().addNewBlock(&stat.loc);
    blk->getLabel(false).setIdent(stat.ident);
    stat.lblock = blk;
    if (stat.tf)
    {
        EmitInBlock(*this, *stat.tf, *blk);
    }
    else if (Statement* s = stat.statement)
    {
        EmitInBlock(*this, *s, *blk);
    }
}


void IRState::toIR(OnScopeStatement& /* stat */)
{
    //nothing to do, the front-end synthesizes the proper try/catch/finally code
}


static bool isVoidRet(FuncDeclaration& funcDecl)
{
    // in MSIL .ctors return void
    if (funcDecl.isCtorDeclaration())
    {
        return true;
    }
    if (funcDecl.csym)
    {
        if (Method* method = funcDecl.csym->isMethod())
        {
            return (DEREF(method->getRetType()).ty == Tvoid);
        }
    }
    return false;
}


void IRState::toIR(ReturnStatement& stat)
{
    if (FuncDeclaration* funcDecl = hasFuncDecl())
    {
        if (funcDecl->hasReturnExp || funcDecl->fes /* foreach statement? */)
        {
            if (stat.loc.linnum == 0)
            {
                // perhaps compiler-synthesized?
                stat.loc = funcDecl->endloc;
            }
            block& blk = getBlock();
            
            if (!stat.exp || isVoidRet(*funcDecl))
            {
                blk.addRet(&stat.loc);
            }
            else
            {
                bool byRef = false;
                if (ret_ && ret_->getDecl().isRef())
                {
                    byRef = true;
                }
                Variable* v = NULL;
                elem& e = loadExp(*this, DEREF(stat.exp), &v, byRef);
                if (e.isASlice() && !toILType(stat.loc, v->getType()).isSliceType())
                {
                    blk.pop_back();
                }
                //check if the current function returns a slice
                //and the tail call returns an array
                if (Method* method = e.isMethodCall())
                {
                    if (ArrayType* aType = toILType(stat.loc, method->getRetType()).isArrayType())
                    {
                        if (!aType->isSliceType() && toILType(stat.loc, ret_->getType()).isSliceType())
                        {
                            add(*new ArrayToSlice(*aType, stat.loc)); // construct slice from array
                        }
                    }
                }
                blk.addRet(&stat.loc, ret_);
            }
        }
    }
    else
    {
        BackEnd::fatalError(stat.loc, "ret statement outside function scope");
    }
}


void IRState::toIR(ThrowStatement& stat)
{
    loadExp(*this, DEREF(stat.exp));
    Instruction::create(getBlock(), IL_throw, &stat.loc);
}


void IRState::toIR(TryCatchStatement& stat)
{
    const unsigned depth = getBlock().depth();
    block* blk = new block(&getBlock(), stat.loc, depth + 1);
    getBlock().add(*blk);
    TryCatchHandler* eh = new TryCatchHandler(blk, stat, depth);
    blk->setEH(eh);
    eh->genCatchHandlers(*this);
    if (stat.body)
    {
        blk = blk->addNewBlock(&stat.loc);
        blk->addEHTranslator();
        EmitInBlock(*this, *stat.body, *blk);
    }
}


void IRState::toIR(TryFinallyStatement& stat)
{
    const unsigned depth = getBlock().depth();
    block* blk = new block(&getBlock(), stat.loc, depth + 1);
    getBlock().add(*blk);
    TryCatchHandler* eh = new TryCatchHandler(blk, stat, depth);
    blk->setEH(eh);
    eh->genFinally(*this, stat.finalbody);
    if (stat.body)
    {
        EmitInBlock(*this, *stat.body, *blk);
    }
}


void IRState::toIR(WithStatement& stat)
{
    block* blk = &getBlock();
    Variable* v = NULL;
    if (stat.wthis)
    {
        Type* type = stat.wthis->type;
        if (type->ty == Tpointer)
        {
            type = static_cast<TypePointer*>(type)->next;
            stat.wthis->type = type;
            stat.wthis->storage_class |= STCref;
        }
        v = createVar(*stat.wthis);        
    }
    if (stat.exp)
    {
        loadExp(*this, *stat.exp);
        if (v)
        {
            v->store(*blk, &stat.loc);
        }
    }
    if (stat.body)
    {
        stat.body->toIR(this);
    }
}

#pragma endregion toIR


void IRState::genWhileLoop(Statement& stat,
                           Expression& condition,
                           bool testCondFirst
                           )
{
    const unsigned depth = getBlock().depth();

    auto_ptr<block> cond(new block(&getBlock(), stat.loc, depth));
    auto_ptr<block> body(new block(&getBlock(), stat.loc, depth));
    auto_ptr<block> done(new block(&getBlock(), stat.loc, depth));

    LoopContext ctxt;
    setLoopContext(ctxt, done.get(), cond.get());

    if (testCondFirst)
    {
        block* test = getBlock().addNewBlock(stat.loc);
        EmitInBlock emitInBlock(*this, *test);
        // unconditionally go to testing condition
        Branch::create(getBlock(), cond.get());
    }
    // emit the loop body
    {   EmitInBlock emitInBlock(*this, *body);
        stat.toIR(this);
    }
    // emit the code that evaluates the condition
    // for staying in the loop
    {   EmitInBlock emitInBlock(*this, *cond);
        elem* evalCond = &loadExp(*this, condition);
        block* dest = (testCondFirst && body->empty()) ? cond.get() : body.get();
        Branch::create(evalCond, getBlock(), dest);
    }
    getBlock().add(body);
    getBlock().add(cond);
    if (done->isReferenced())
    {
        getBlock().add(done);
    }
}
