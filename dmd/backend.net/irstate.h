#ifndef IRSTATE_H__2DE5F625_7D2E_42CE_A9B7_0813883DCEA6
#define IRSTATE_H__2DE5F625_7D2E_42CE_A9B7_0813883DCEA6
// -*- tab-width: 4; indent-tabs-mode: nil;  -*-
// vim: tabstop=4:softtabstop=4:expandtab:shiftwidth=4
//
// $Id: irstate.h 24625 2009-07-31 01:05:55Z unknown $
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
#include <iosfwd>
#include <map>
#include "expression.h"
#include "backend.net/block.h"
#include "backend.net/modemit.h"
#include "backend.net/msil/mnemo.h"

#ifndef _MSC_VER
 #define sealed
#endif
/****************************/
#pragma region Forward Decls
struct AggregateDeclaration;
struct AsmStatement;
struct BreakStatement;
struct ContinueStatement;
struct DoStatement;
struct DefaultStatement;
struct ExpStatement;
struct ForStatement;
struct ForeachRangeStatement;
struct ForeachStatement;
struct GotoCaseStatement;
struct GotoDefaultStatement;
struct IfStatement;
struct Initializer;
struct LabelStatement;
struct OnScopeStatement;
struct ScopeStatement;
struct SwitchStatement;
struct CaseStatement;
struct ReturnStatement;
struct SynchronizedStatement;
struct SwitchErrorStatement;
struct ThrowStatement;
struct TryCatchStatement;
struct TryFinallyStatement;
struct VarDeclaration;
struct UnrolledLoopStatement;
struct WhileStatement;
struct WithStatement;

class Method;   // backend
class HiddenMethodStatement;

#pragma endregion
/****************************/

struct LoopContext
{
    Temporary<block*> break_;   // where to break out
    Temporary<block*> cont_;    // where to continue
};


/// Encapsulate access to the block where elements
/// are currently being inserted.
class IRBase
{
    block* block_;
    std::vector<block*> stack_; // for saving current block

protected:
    explicit IRBase(block& blk) : block_(&blk) { }
   
public:
    // Set the insertion point for future elems
    void setBlock(block& block) { block_ = &block; }

    inline block& getBlock() const { return DEREF(block_); }

    void push() { stack_.push_back(block_); }
    void pop();
};


///<summary>
/// Emit Intermediate Representation code.
///</summary>
///
/// There is one IRState instance per function scope 
/// and another instance at the module scope. Temporary IRState
/// objects may be constructed as needed.
///
/// Perhaps IREmitter would have been a better name,
/// but IRState is what the front-end expects.
///
struct IRState sealed : public IRBase
{
    typedef std::map<Identifier*, block*> LabeledBlocks;

private:
    IRState(const IRState&); // non-copyable
    IRState& operator=(const IRState&); // non-assignable

    void setLoopContext(LoopContext&, block* brk, block* cont, block* current = NULL);

    void handleConstCases(SwitchStatement&, block&, std::auto_ptr<block> defaultBlock);
    void handleVarCases(SwitchStatement&, std::auto_ptr<block> defaultBlock);

public:
    class StateSaver
    {
        IRState& state_;
    public:
        explicit StateSaver(IRState& state) : state_(state) { state.push(); }
        ~StateSaver() { state_.pop(); }
    };

    ///<summary>
    /// ctor takes the block to build; IRState does not manage block's memory
    ///</summary>
    explicit IRState(block&);

    // no need for virtual dtor-- class is sealed
    /* virtual */ ~IRState();

    void setDecl(AggregateDeclaration* decl);
    void setDecl(EnumDeclaration* decl);
    void setDecl(FuncDeclaration* decl);
    void setDecl(VarDeclaration* decl);

    AggregateDeclaration* hasAggregateDecl();
    ClassDeclaration* hasClassDecl();
    EnumDeclaration* hasEnumDecl();
    FuncDeclaration* hasFuncDecl();
    VarDeclaration* hasVarDecl();

    Variable* createVar(VarDeclaration&);
    Variable* createTempVar(VarDeclaration&, Type*);

    void emitInBlock(block&, Statement&);

    elem* add(elem& e) { return getBlock().add(e); }
    elem* addInit(elem* e) { return add(*new dt_t(e)); }

    // get / set variable used to temporarily store the return value
    Variable* getRetVar() const { return ret_; }
    void setRetVar(Variable* var) { assert(!ret_); ret_ = var; }

    Method* inCall() const { return inCall_; }

    // return the current foreach aggregate, or NULL
    Expression* foreachAggr() const { return foreachAggr_; }
    void setForeachAggr(Expression* aggr)
    {
        foreachAggr_ = aggr;
    }

    // Visitor pattern
#define TO_ELEM_IMPL(E) elem* toElem(E&);
    TO_ELEM_IMPL(AddAssignExp)
    TO_ELEM_IMPL(AddExp)
    TO_ELEM_IMPL(AddrExp)
    TO_ELEM_IMPL(AndAndExp)
    TO_ELEM_IMPL(AndAssignExp)
    TO_ELEM_IMPL(AndExp)
    TO_ELEM_IMPL(ArrayLengthExp)
    TO_ELEM_IMPL(ArrayLiteralExp)
    TO_ELEM_IMPL(AssertExp)
    TO_ELEM_IMPL(AssignExp)
    TO_ELEM_IMPL(AssocArrayLiteralExp)
    TO_ELEM_IMPL(BoolExp)
    TO_ELEM_IMPL(CallExp)
    TO_ELEM_IMPL(CastExp)
    TO_ELEM_IMPL(CatAssignExp)
    TO_ELEM_IMPL(CatExp)
    TO_ELEM_IMPL(CmpExp)
    TO_ELEM_IMPL(ComExp)
    TO_ELEM_IMPL(CommaExp)
    TO_ELEM_IMPL(ComplexExp)
    TO_ELEM_IMPL(CondExp)
    TO_ELEM_IMPL(DeclarationExp)
    TO_ELEM_IMPL(DelegateExp)
    TO_ELEM_IMPL(DeleteExp)
    TO_ELEM_IMPL(DivAssignExp)
    TO_ELEM_IMPL(DivExp)
    TO_ELEM_IMPL(DotTypeExp)
    TO_ELEM_IMPL(DotVarExp)
    TO_ELEM_IMPL(EqualExp)
    TO_ELEM_IMPL(Expression)
    TO_ELEM_IMPL(FuncExp)
    TO_ELEM_IMPL(HaltExp)
    TO_ELEM_IMPL(IdentityExp)
    TO_ELEM_IMPL(IndexExp)
    TO_ELEM_IMPL(InExp)
    TO_ELEM_IMPL(IntegerExp)
    TO_ELEM_IMPL(MinAssignExp)
    TO_ELEM_IMPL(MinExp)
    TO_ELEM_IMPL(ModAssignExp)
    TO_ELEM_IMPL(ModExp)
    TO_ELEM_IMPL(MulAssignExp)
    TO_ELEM_IMPL(MulExp)
    TO_ELEM_IMPL(NegExp)
    TO_ELEM_IMPL(NewExp)
    TO_ELEM_IMPL(NotExp)
    TO_ELEM_IMPL(NullExp)
    TO_ELEM_IMPL(OrAssignExp)
    TO_ELEM_IMPL(OrExp)
    TO_ELEM_IMPL(OrOrExp)
    TO_ELEM_IMPL(PostExp)
    TO_ELEM_IMPL(PtrExp)
    TO_ELEM_IMPL(RealExp)
    TO_ELEM_IMPL(RemoveExp)
    TO_ELEM_IMPL(ScopeExp)
    TO_ELEM_IMPL(ShlAssignExp)
    TO_ELEM_IMPL(ShlExp)
    TO_ELEM_IMPL(ShrAssignExp)
    TO_ELEM_IMPL(ShrExp)
    TO_ELEM_IMPL(SliceExp)
    TO_ELEM_IMPL(StringExp)
    TO_ELEM_IMPL(StructLiteralExp)
    TO_ELEM_IMPL(SymbolExp)
    TO_ELEM_IMPL(ThisExp)
    TO_ELEM_IMPL(TupleExp)
    TO_ELEM_IMPL(TypeExp)
    TO_ELEM_IMPL(UshrAssignExp)
    TO_ELEM_IMPL(UshrExp)
    TO_ELEM_IMPL(XorAssignExp)
    TO_ELEM_IMPL(XorExp)
#undef TO_ELEM_IMPL
    elem* toElem(SymOffExp&, Variable&);

    void toIR(AsmStatement&);
    void toIR(BreakStatement&);
    void toIR(CaseStatement&);
    void toIR(ContinueStatement&);
    void toIR(DefaultStatement&);
    void toIR(DoStatement&);
    void toIR(ExpStatement&);
    void toIR(ForStatement&);
    void toIR(ForeachRangeStatement&);
    void toIR(ForeachStatement&);
    void toIR(GotoCaseStatement&);
    void toIR(GotoDefaultStatement&);
    void toIR(HiddenMethodStatement&); // synthesized by back-end
    void toIR(IfStatement&);
    void toIR(LabelStatement&);
    void toIR(OnScopeStatement&);
    void toIR(ReturnStatement&);
    void toIR(SynchronizedStatement&);
    void toIR(SwitchErrorStatement&);
    void toIR(SwitchStatement&);
    void toIR(ThrowStatement&);
    void toIR(TryCatchStatement&);
    void toIR(TryFinallyStatement&);
    void toIR(UnrolledLoopStatement&);
    void toIR(WhileStatement&);
    void toIR(WithStatement&);

private:
    elem* toElemImpl(NewExp&);

    ///<summary>
    ///Generate code for the arguments of the binary expression
    ///</summary>
    ///<return> lvalue, if applicable, or NULL </return>
    ///isAssignment indicates whether the result is to be stored
    ///back into the left hand-side expression (as in "x += y");
    ///if "prev" is not NULL, upon return it points to a temp variable
    ///that saves the value of the left hand-side before the binary
    ///operation, useful for implementing postfix incr. and decrement
    Variable* genBinaryArgs(BinExp&, 
                            bool isAssignment = false,
                            Variable** prev = NULL
                            );

    elem* createBinaryOp(BinExp&,
                         Mnemonic,
                         Variable** lval = NULL,
                         bool isAssignment = false,
                         Variable** prev = NULL
                         );
    elem* createAssignOp(BinExp&, Mnemonic, Variable** prev = NULL);
    elem* createLogicalOp(BinExp&, Mnemonic);

    /// Helper called from toElem(AssignExp&)
    void arrayAssign(AssignExp&, Variable&, elem& rhs, ArrayType&);

    /// Prepare arguments for call, return method to call
    Method& prepCall(Loc&, Expressions* args, Symbol* callee);

    /// Generate while and do ... while loops
    /// If testCondFirst is true, it is a while loop, otherwise
    /// it's a do ... while
    void genWhileLoop(Statement&, Expression& cond, bool testCondFirst);

    void foreachArrayElem(ForeachStatement&,
                          ArrayType&,
                          Variable& arr,
                          Variable& key,
                          Variable* val);
    elem* handleDollarSymbol(SymbolExp&);
    
    // handles  default case of a switch statement;
    void fallThroughDefault(std::auto_ptr<block> sdefault);

private:
    Dsymbol*    decl_;
    block*      break_;     // for "break" statements
    block*      cont_;      // for "continue" statements
    block*      default_;   // for switch default cases
    Variable*   ret_;       // temporary to store return value
                            //  if exception handling is needed
    Variable*   array_;     // context for IndexExp and SliceExp
    LabeledBlocks labeledBreak_;
    LabeledBlocks labeledCont_;

    Encoding    encoding_;  // current string encoding
    Method*     inCall_;
    Expression* foreachAggr_;
};

#endif // IRSTATE_H__2DE5F625_7D2E_42CE_A9B7_0813883DCEA6
