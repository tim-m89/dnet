#ifndef MODGEN_H__DC2A706E_2AA4_4C65_93E2_1EB56D77D463
#define MODGEN_H__DC2A706E_2AA4_4C65_93E2_1EB56D77D463
// -*- tab-width: 4; indent-tabs-mode: nil;  -*-
// vim: tabstop=4:softtabstop=4:expandtab:shiftwidth=4
//
// $Id: modemit.h 24625 2009-07-31 01:05:55Z unknown $
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
#include <vector>
#include "backend.net/backend.h"
#include "backend.net/msil/typedef.h"

/****************************/
#pragma region Forward Decls
struct ClassDeclaration;
struct EnumDeclaration;
struct FuncDeclaration;
struct IRState;
struct TypeInfoDeclaration;
struct TYPE;
struct VarDeclaration;

//backend
class Assembly;
class Class;
class Field;
class Method;
#pragma endregion
/****************************/


//throws if decl.csym is not a method
Method& getMethod(Loc& loc, FuncDeclaration& decl);


class ModuleEmitter
{
    static bool emitAssembly_;
    static ModuleEmitter* current_;
    ModuleEmitter* prev_;
    Module& module_;                // D Module
    Assembly& assembly_;
    block* block_;                  // the body of code
    block* tls_;
    block* staticInitBlock_;
    block* staticFields_;
    block* typedefs_;
    IRState* irState_;              // state at this module scope
    std::vector<IRState*> state_;   // state stack
    bool verifiable_;
    mutable std::string dataSection_;
    std::set<DefineDirective> defines_;

private:
    //non-copyable, non-assignable
    ModuleEmitter(const ModuleEmitter&);
    ModuleEmitter& operator=(const ModuleEmitter&);
    
    void emitIL(std::wostream&);
    block& getTlsBlock(Loc&);
    block& getStaticFields();

    void genInitializer(IRState&, VarDeclaration&, TYPE&);
    void emitStaticInit(std::wostream&);
    
    const std::string& getDataSectionName() const;

public:
    ModuleEmitter(Assembly&, Module&);
    virtual ~ModuleEmitter();

    /// Get the Emitter for the current Module scope
    static ModuleEmitter* getCurrent() { return current_; }

    const Module& getModule() const { return module_; }
    Module& getModule() { return module_; }

    /// Get the top state object associated with this module
    IRState& getIRState() { return DEREF(irState_); }

    block& getBlock() const { return DEREF(block_); }
    block& getStaticInitBlock();
    block& getTypedefs() { return DEREF(typedefs_); }
    bool add(DefineDirective&);

#if TLS_DATA_SLOT
    ///make thread local storage for given variable
    dt_t* createTls(Field&);
#endif
    /******** Manage state for module elements ************/
    void push(IRState& s) { state_.push_back(&s); }
    void pop(IRState& s)
    {
        assert(!state_.empty() && state_.back() == &s);
        state_.pop_back();
    }
    void push();
    void pop();

    IRState* getCurrentIRState() const
    {
        return state_.empty() ? irState_ : state_.back();
    }
    FuncDeclaration* getCurrentFuncDeclaration() const;
    Method& getCurrentMethod(Loc&) const;

    /*************** Handle declarations ******************/
    void createFunction(FuncDeclaration&);
    Method* createMethod(block&, FuncDeclaration&, bool = false, Dsymbol* = NULL);

    Field* createGlobalVar(VarDeclaration&, IRState* = NULL);
    ///<summary>
    /// Create a variable in the current scope
    ///</summary>
    void createVar(VarDeclaration&);

    ///<summary>
    /// Create a per-module variable that contains some type info
    ///</summary>
    void createVar(TypeInfoDeclaration&);

    ///<summary>
    /// Introduce a class declaration in the current scope
    ///</summary>
    Class* createClass(ClassDeclaration&);

    void createEnum(EnumDeclaration&);
    void createStruct(StructDeclaration&);

    void emitIL(const char* filename);

    void setUnverifiable(Loc&);
};

#endif // MODGEN_H__DC2A706E_2AA4_4C65_93E2_1EB56D77D463

