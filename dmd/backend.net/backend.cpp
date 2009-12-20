// -*- tab-width: 4; indent-tabs-mode: nil;  -*-
// vim: tabstop=4:softtabstop=4:expandtab:shiftwidth=4
//
// $Id: backend.cpp 24625 2009-07-31 01:05:55Z unknown $
//
// Copyright (c) 2008 Cristian L. Vlasceanu

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
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include "aggregate.h"
#include "declaration.h"
#include "init.h"               // Initializer, ExpInitializer
#include "mars.h"
#include "module.h"
#include "scope.h"
#include "backend.net/backend.h"
#include "backend.net/irstate.h"
#include "backend.net/modemit.h"
#include "backend.net/utils.h"
#include "backend.net/msil/assembly.h"
#include "backend.net/msil/const.h"
#include "backend.net/msil/classemit.h"
#include "backend.net/msil/instruction.h"
#include "backend.net/msil/member.h"
#include "backend.net/msil/types.h"

//uncomment to include D source in IL output
// #define OUTPUT_SOURCE_AS_COMMENTS 1


using namespace std;


BackEnd* BackEnd::instance_ = NULL;
bool g_haveEntryPoint = false;
bool g_haveUnverifiableCode = false;
bool g_havePointerArithmetic = false;


/***********************************
 * Include D source lines as comments
 * in the generated IL, to ease up the
 * debugging of our code generator.
 */
static vector<string> currentSource;

static void readSource(const string& fname)
{
#if OUTPUT_SOURCE_AS_COMMENTS
    ifstream f(fname.c_str());
    if (!f) throw runtime_error("could not read source: " + fname);
    currentSource.clear();
    currentSource.push_back(fname);
    char line[1024]; 
    while (f.getline(line, sizeof line))
    {
        currentSource.push_back(line);
    }
#endif
}


/// Print source lines from the currentSource vector,
/// starting with 'first' and up to 'last'
static void printSource(const elem& e,
                        wostream& out, 
                        unsigned first, 
                        unsigned last
                        )
{
#if OUTPUT_SOURCE_AS_COMMENTS
    if  ((first > last) || (first == 0))
    {
        first = last;
        last = first + 1;
    }
    else
    {
        ++first, ++last;
    }
    if (first != last)
    {
        e.indent(out) << "// " << currentSource[0].c_str() << ":" << first << "\n";
    }
    for (; first != last; ++first)
    {
        if (first >= currentSource.size())
        {
            break;
        }
        e.indent(out) << "// " << currentSource[first].c_str() << "\n";
    }
#endif
}


void backend_init()
{
    //
    // todo: some of the front-end command line switches
    // may not make sense for generating MSIL -- revisit
    //
    global.obj_ext = "il";
   
    BackEnd::instance_ = new BackEnd();
}


void backend_term()
{
    delete BackEnd::instance_;
    assert(BackEnd::instance_ == 0);
}


void util_progress()
{
}


BackEnd::BackEnd() : line_(0), labelCount_(0)
{
}


BackEnd::~BackEnd()
{
    // check for leaked elem's
    assembly_.reset();
    assert(elem::getCount() == 0);

    assert(instance_);
    instance_ = NULL;
}


auto_ptr<ModuleEmitter> BackEnd::getModule(Module& module)
{
    return auto_ptr<ModuleEmitter>(getModule(module, true));
}


//This method is useful when we need to emit a symbol in an imported
//module. If the module also participates in the compilation, it will
//be retrieved from the ModuleMap at a later time.
ModuleEmitter* BackEnd::getModule(Module& module, bool remove)
{
    if (assembly_.get() == NULL)
    {
        assembly_.reset(new Assembly(module.toChars()));
    }
    ModuleEmitter* result = NULL;
    
    const string name(module.arg);
    ModuleMap::iterator i = moduleMap_.find(name);
    if (i == moduleMap_.end())
    {
        result = new ModuleEmitter(*assembly_, module);
        if (!remove)
        {
            moduleMap_.insert(i, make_pair(name, result));
        }
    }
    else
    {
        result = i->second;
        result->push();
        if (remove)
        {
            moduleMap_.erase(i);
        }
    }
    return result;
}


ModuleEmitter& BackEnd::getModuleScope(Loc& loc)
{
    ModuleEmitter* modScope = ModuleEmitter::getCurrent();
    if (!modScope)
    {
        //must always be in the scope of a module
        fatalError(loc, "outside module scope");
    }
    return *modScope;
}


Method& BackEnd::getCurrentMethod(Loc& loc)
{
    ModuleEmitter& mod = getModuleScope(loc);
    return mod.getCurrentMethod(loc);
}


bool BackEnd::inNestedVarScope(Loc& loc)
{
    return getCurrentMethod(loc).getVarScope() != NULL;
}


void BackEnd::fatalError(Loc& loc, const char* msg)
{    
    string prefix;
    if (const char* p = loc.toChars())
    {
        prefix = p;
        prefix += ": ";
    }
    if (msg)
    {
        prefix += msg;
        prefix += " -- ";
    }
    throw std::runtime_error(prefix + "code generation cannot continue");
}


///<summary>
/// Get the IRState object and visit expr.
///</summary>
elem* BackEnd::toElem(Expression& expr)
{
    ModuleEmitter& modScope = getModuleScope(expr.loc);
    if (IRState* irstate = modScope.getCurrentIRState())
    {
        elem* e = expr.toElem(irstate);
        assert(e);
        assert(e->getOwner());
        return e;
    }
    fatalError(expr.loc, "null IR state");
    return NULL;
}


bool BackEnd::emitLineInfo(const elem& e, Loc& loc, std::wostream& out) const
{
    if (global.params.debuglevel || global.params.symdebug)
    {
        bool fileChanged = (loc.filename && currentSourceFileName_ != loc.filename);
        {
            if (loc.linnum)
            {
                e.indent(out) << ".line " << loc.linnum;
            }
            else
            {
                e.indent(out) << "// synthesized";
            }
            if (fileChanged)
            {
                std::string tmp(loc.filename);
                readSource(tmp);
                currentSourceFileName_ = loc.filename;
                replaceBackSlashes(tmp);
                out << " '" << tmp.c_str() << "'";
            }
            out << "\n";
        }
        printSource(e, out, line_, loc.linnum);
        line_ = loc.linnum;
        return true;
    }
    return false;
}


wstring BackEnd::newLabel() const
{
    wostringstream buf;
    buf << "L" << labelCount_++ << "_" << assembly_->getName().c_str();
    return buf.str();
}


void BackEnd::resolve(FuncDeclaration& decl)
{
    assert(!decl.csym);

    ModuleEmitter& modScope = getModuleScope(decl.loc);
    // is the function declared in the scope of the current module?
    if (decl.scope && decl.scope->module == &modScope.getModule())
    {
        if (ClassDeclaration* classDecl = decl.isClassMember())
        {
            Class& c = static_cast<Class&>(DEREF(classDecl->csym));
            modScope.createMethod(c.getBlock(), decl);
        }
        else
        {
            modScope.createFunction(decl);
        }
    }
    else
    {
        // create a method at the assembly level (where it does not get emitted)
        Method* method = new Method(decl, 0);
        getAssembly().add(*method);
        decl.csym = method;
        if (decl.ident == Id::eq)
        {
            //force dispatch thru the System.Object's vtable
            method->setProto(L"bool object::Equals(object)", 1);
        }
    }
}


// Generate code that initializes the elements of an array.
// Expects an array ref on the top of the stack.
dt_t* BackEnd::toDt(ArrayInitializer& init)
{
    ModuleEmitter& modScope = getModuleScope(init.loc);
    IRState* irstate = modScope.getCurrentIRState();
    if (!irstate)
    {
        fatalError(init.loc, "null IR state");
    }
    irstate->getBlock();
    VarDeclaration* varDecl = irstate->hasVarDecl();
    if (!varDecl)
    {
        BackEnd::fatalError(init.loc, "initializer expects var decl");
    }
    TYPE& type = toILType(init.loc, varDecl->type);
    ArrayType* aType = type.isArrayType();
    if (!aType)
    {
        BackEnd::fatalError(init.loc, "initializer expects array type");
    }
    aType->init(*irstate, *varDecl, init.dim);
    block& blk = irstate->getBlock();

    IRState temp(blk);
    ArrayAdapter<Initializer*> value(init.value);
    ArrayAdapter<Expression*> index(init.index);
    //
    // and here's where the elements are initialized:
    //
    for (size_t i = 0; i != init.dim; ++i)
    {
        if (i >= value.size() || i >= index.size())
        {
            assert(false);
            break;
        }
        if (i + 1 != init.dim)
        {
            // For each element duplicate the reference to the array
            // on the top of the stack, so that one is left on the stack for
            // the next element to use -- except when reaching the last element
            Instruction::create(blk, IL_dup);
        }
    #if 0
        //todo:revisit, indices may be null, what are they useful for anyway?
        DEREF(index[i]).toElem(irstate);
    #else
        Const<int>::create(*TYPE::Int32, i, blk, init.loc);
    #endif
        auto_ptr<dt_t>(DEREF(value[i]).toDt());
        ArrayElemAccess::store(blk, aType->elemTYPE());
    }
    return NULL;
}


//operator for keying Array types by element type
bool BackEnd::ArrayTypeLess::operator ()(const ArrayType* lhs, const ArrayType* rhs) const
{
    assert(lhs);
    assert(rhs);
    return lhs->elemType().ty < rhs->elemType().ty;
}


SliceType* BackEnd::getSliceType(Loc& loc, ArrayType& type)
{
    ArraySliceTypes::iterator i = arraySliceTypes_.find(&type);
    if (i == arraySliceTypes_.end())
    {
        i = arraySliceTypes_.insert(i, new SliceType(loc, type.elemType()));
    }
    SliceType* sliceType = (*i)->isSliceType();
    assert(sliceType);
    getAssembly().addSliceTypedef();
    return sliceType;
}



dt_t* BackEnd::construct(Loc& loc, StructType& sType)
{
    ModuleEmitter& modScope = getModuleScope(loc);
    if (IRState* irstate = modScope.getCurrentIRState())
    {
        if (VarDeclaration* vdecl = irstate->hasVarDecl())
        {
            Symbol* csym = vdecl->csym;
            if (csym && csym->refCount() == 0)
            {
                if (Variable* v = csym->isVariable())
                {
#if VALUETYPE_STRUCT
                    // generate code to call Struct constructor in place
                    v->load(irstate->getBlock(), &loc, true);
                    new DefaultCtorCall<StructType>(irstate->getBlock(), IL_call, sType);
#else
                    new DefaultCtorCall<StructType>(irstate->getBlock(), IL_newobj, sType);
                    v->store(irstate->getBlock(), &loc);
#endif
                }
            }
        }
    }
    return NULL;
}



bool BackEnd::addExtern(const ExternAssembly& externAssembly)
{
    if (assembly_.get() == NULL)
    {
        assembly_.reset(new Assembly(DEREF(Module::rootModule).toChars()));
    }
    return assembly_->addExtern(externAssembly);
}

