#ifndef BACKEND_H__8F8D93EE_9F4D_4626_81AB_BE87B4C6DAF1
#define BACKEND_H__8F8D93EE_9F4D_4626_81AB_BE87B4C6DAF1
// -*- tab-width: 4; indent-tabs-mode: nil;  -*-
// vim: tabstop=4:softtabstop=4:expandtab:shiftwidth=4
//
// $Id: backend.h 24625 2009-07-31 01:05:55Z unknown $
//
// Copyright (c) 2008 - 2009 Cristian L. Vlasceanu

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
#if !TARGET_NET
 #error TARGET_NET must be defined
#endif
#include <memory>
#include <map>
#include <set>
#include <stdexcept>
#include "backend.net/elem.h"
#include "backend.net/utils.h"

/****************************/
#pragma region Forward Decls
class ExternAssembly;

struct ArrayInitializer;
struct Declaration;
struct Dsymbol;
struct Expression;
struct FuncDeclaration;
struct Initializer;
struct Loc;
struct Module;
struct Scope;
struct VarDeclaration;
//backend
class ArrayType;
class Assembly;
class Method;
class ModuleEmitter;
class SliceType;
class StructType;
#pragma endregion
/****************************/

extern bool g_haveEntryPoint;
extern bool g_haveUnverifiableCode;
extern bool g_havePointerArithmetic;


///<summary>
/// BackEnd access point.
///</summary>
class BackEnd
{
    struct ArrayTypeLess : public std::binary_function<
        const ArrayType*,
        const ArrayType*,
        bool>
    {
        bool operator()(const ArrayType*, const ArrayType*) const;
    };
    typedef std::set<ArrayType*, ArrayTypeLess> ArraySliceTypes;
    typedef std::map<std::string, ModuleEmitter*> ModuleMap;

    BackEnd();
    ~BackEnd();

    BackEnd(const BackEnd&); // non-copyable    
    BackEnd& operator=(const BackEnd&); // non-assignable

    static BackEnd* instance_;
    std::auto_ptr<Assembly> assembly_;
    mutable std::string currentSourceFileName_;
    mutable unsigned line_;
    mutable size_t labelCount_; // for generating unique labels
    mutable ArraySliceTypes arraySliceTypes_;
    ModuleMap moduleMap_;

public:
    friend void backend_init();
    friend void backend_term();

    static BackEnd& instance()
    {
        if (!instance_)
        {
            throw std::logic_error("BackEnd::instance is null");
        }
        return *instance_;
    }

    static ModuleEmitter& getModuleScope(Loc&);
    static Method& getCurrentMethod(Loc&);
    static bool inNestedVarScope(Loc&);
    static const char* name() { return "D.NET"; }
    static void fatalError(Loc& sourceLocation, const char* = "");

    static elem* toElem(Expression&);
    static dt_t* toDt(ArrayInitializer&);
    static dt_t* construct(Loc&, StructType&);

    Assembly& getAssembly() { return DEREF(assembly_.get()); }
    std::auto_ptr<ModuleEmitter> getModule(Module&);
    ModuleEmitter* getModule(Module&, bool remove);

    void resolve(FuncDeclaration&);
    std::wstring newLabel() const; 

    bool emitLineInfo(const elem&, Loc&, std::wostream&) const;
    const std::string& getCurrentSourceFileName() const
    {
        return currentSourceFileName_;
    }
    SliceType* getSliceType(Loc&, ArrayType&);

    bool addExtern(const ExternAssembly&);
};

#endif // BACKEND_H__8F8D93EE_9F4D_4626_81AB_BE87B4C6DAF1
