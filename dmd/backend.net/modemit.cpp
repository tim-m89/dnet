// -*- tab-width: 4; indent-tabs-mode: nil;  -*-
// vim: tabstop=4:softtabstop=4:expandtab:shiftwidth=4
//
// $Id: modemit.cpp 24923 2009-08-06 04:26:28Z unknown $
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
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <time.h>
#include "aggregate.h"
#include "declaration.h"
#include "enum.h"
#include "init.h"
#include "mars.h"       // Global
#include "module.h"
#include "statement.h"
#include "template.h"
#include "backend.net/modemit.h"
#include "backend.net/type.h"
#include "backend.net/irstate.h"
#include "backend.net/msil/assembly.h"
#include "backend.net/msil/classemit.h"
#include "backend.net/msil/enumemit.h"
#include "backend.net/msil/const.h"
#include "backend.net/msil/member.h"
#include "backend.net/msil/structemit.h"
#include "frontend.net/pragma.h"

using namespace std;


ModuleEmitter* ModuleEmitter::current_ = NULL;
bool ModuleEmitter::emitAssembly_ = true;


Symbol* Module::toModuleArray() { return /* NOT_IMPLEMENTED */(NULL); }
Symbol* Module::toModuleAssert(){ return /* NOT_IMPLEMENTED */(NULL); }


// This hack allows specifying the name of an imported assembly
// without modifying (no extra keywords needed) the front-end.
// Return true if the module is "object.d"
static bool getAssemblyName(Module& m)
{
    bool isObject = false;
    if (strcmp(m.toChars(), "object") == 0)
    {
        m.ident = Lexer::idPool("[dnetlib]core");
        isObject = true;
    }
    return isObject;
}




//The order of static initialization is implicitly determined by the import declarations
//in each module. Each module is assumed to depend on any imported modules being statically
//constructed first.
static void examineImports(Module& module, set<Module*>& visited)
{
    ArrayAdapter<Module*> imports(module.aimports);    
    for (ArrayAdapter<Module*>::iterator i = imports.begin(); i != imports.end(); ++i)
    {
        Module* import = *i;

        if (!import->isModule() || !visited.insert(import).second)
        {
            continue;
        }
        if (*import->toChars() != '[')
        {
            getAssemblyName(*import);
        }
        examineImports(*import, visited);

        ArrayAdapter<Dsymbol*> members(import->members);
        for (ArrayAdapter<Dsymbol*>::iterator s = members.begin(); s != members.end(); ++s)
        {
            FuncDeclaration* fd = (*s)->isStaticCtorDeclaration();
            if (!fd)
            {
                fd = (*s)->isStaticDtorDeclaration();
            }
            if (fd && !fd->csym)
            {
                ModuleEmitter* modEmit = BackEnd::instance().getModule(*import, false);
                modEmit->createFunction(*fd);
                modEmit->pop(); //return to the current module context
            }
        }
    }
}



void Module::genobjfile(int multiobj)
{
    try
    {
        auto_ptr<ModuleEmitter> emitter = BackEnd::instance().getModule(*this);
        set<Module*> visited;
        examineImports(*this, visited);

        ArrayAdapter<Dsymbol*> syms(members);
        for (ArrayAdapter<Dsymbol*>::iterator i = syms.begin(); i != syms.end(); ++i)
        {
            Dsymbol* dsym = *i;
            DEREF(dsym).toObjFile(multiobj);
        }
        const char* filename = multiobj 
            ? objfile->name->toChars() 
            : (const char*)global.params.objfiles->data[0];

        emitter->emitIL(filename);
    }
    catch (const exception& e)
    {
        ::error(e.what());
    }
}


ModuleEmitter::ModuleEmitter(Assembly& assembly, Module& module)
    : module_(module)
    , assembly_(assembly)
    , block_(new block(NULL, module.loc, 0))
    , tls_(NULL)
    , staticInitBlock_(NULL)
    , staticFields_(NULL)
    , typedefs_(NULL)
    , irState_(NULL)
    , verifiable_(true)
{
    typedefs_ = block_->addNewBlock();
    prev_ = current_;
    current_ = this;
    assembly.add(*block_);
    irState_ = new IRState(*block_);
}


ModuleEmitter::~ModuleEmitter()
{
    delete irState_;
    // no need to delete block_, assembly_ owns it!

    assert(state_.empty());
    current_ = prev_;
}


void ModuleEmitter::push()
{ 
    prev_ = current_;
    current_ = this;
    push(DEREF(irState_)); 
}


void ModuleEmitter::pop()
{ 
    pop(DEREF(irState_));    
    current_ = prev_;
}


block& ModuleEmitter::getStaticInitBlock() 
{
    if (staticInitBlock_ == NULL)
    {
        staticInitBlock_ = new block(NULL, module_.loc, 2);

        //add to assembly for memory management purposes only;
        //the assembly main block is not emitted, we will emit
        //the static fields explicitly
        assembly_.add(*staticInitBlock_);
    }
    return *staticInitBlock_;
}


block& ModuleEmitter::getStaticFields() 
{
    if (staticFields_ == NULL)
    {
        staticFields_ = new block(NULL, module_.loc, 1);
        //add to assembly for memory management only; the
        //assembly main block is not emitted, we will emit
        //the static fields explicitly
        assembly_.add(*staticFields_);
    }
    return *staticFields_;
}


const string& ModuleEmitter::getDataSectionName() const
{    
    if (dataSection_.empty())
    {
        dataSection_ = module_.toChars();
        dataSection_ += ".$data";
    }
    return dataSection_;
}


Method* ModuleEmitter::createMethod(block& block,
                                    FuncDeclaration& funcDecl,
                                    bool nested,
                                    Dsymbol* parent
                                    )
{
    Method* method = NULL;
    if (funcDecl.csym == NULL)
    {
        method = new Method(funcDecl, block.depth(), nested, parent);
        block.add(*method);
        funcDecl.csym = method;
    }
    else if ((method = funcDecl.csym->isMethod()) != NULL)
    {
        // imported but not emitted? (imported methods are "hidden" in
        // the assembly block)
        if (DEREF(method->getOwner()).isAssembly())
        {
            method = new Method(funcDecl, block.depth(), nested, parent);
            funcDecl.csym = method;
            block.add(*method);
        }
    }
    else
    {
        BackEnd::fatalError(funcDecl.loc, "decl has non-method symbol attached");
    }
    return method;
}


void ModuleEmitter::createEnum(EnumDeclaration& decl)
{
    block& block = DEREF(irState_).getBlock();
    Enum* e = new Enum(decl, block.depth());
    block.add(*e);
    assert(decl.csym == e);
}


//Return true if the function is a template method.
//Generate the body of the method inside the class that owns it.
static bool handleTemplateMethod(ModuleEmitter& modEmit, FuncDeclaration& funcDecl)
{
    bool result = false;

    if (TemplateInstance* tmpl = funcDecl.inTemplateInstance())
    {
        if (AggregateDeclaration* decl = tmpl->isMember())
        {
            //Generate template member methods inside their classes
            Aggregate& a = getAggregate(*decl);
            IRState temp(a.getBlock());
            temp.setDecl(decl);

            funcDecl.parent = decl; // re-parent function

            if (Method* m = modEmit.createMethod(a.getBlock(), funcDecl))
            {
                m->setTemplate();
                m->generateBody();
            }
            result = true;
        }
    }
    return result;
}


//Called by ModuleEmitter::createFunction
//The idea: a nested function needs to "see" the surrounding lexical
//context. To achieve this, the accessed variables are transmitted as
//fields of a class instance.
//If the parent of the nested function is a global function, then we
//synthesize the class; otherwise we just add the fields to the class
//that the outer function is a member of.
static AggregateDeclaration* handleNested(
        ModuleEmitter& modEmit,
        FuncDeclaration& funcDecl,
        bool& nested,
        bool& member
        )
{
    AggregateDeclaration* aggrDecl = NULL;
    if (funcDecl.isNested())
    {
        //we maintain our own "nested" flag because once we mess with
        //funcDecl.parent (see below), isNested() will return false
        nested = true;

        Method& currentFunc = modEmit.getCurrentMethod(funcDecl.loc);
        aggrDecl = currentFunc.getAggregateDecl();
        if (aggrDecl)
        {
            member = true; // funcDecl is class / struct member function
        }
        else
        {
            // function is non-member, make a closure class
            Identifier* ident = Identifier::generateId("Closure");
            //synthesize a class declaration
            ClassDeclaration* classDecl = new ClassDeclaration(funcDecl.loc, ident, NULL);
            classDecl->protection = PROTprivate;
            classDecl->baseClass = ClassDeclaration::object;
            classDecl->parent = funcDecl.parent;
            aggrDecl = classDecl;
            modEmit.createClass(*classDecl);
        }
        funcDecl.parent = aggrDecl;
        funcDecl.vthis = new VarDeclaration(funcDecl.loc,
                                            DEREF(aggrDecl->type).pointerTo(),
                                            Lexer::idPool("this"),
                                            NULL);
    }
    return aggrDecl;
}


void ModuleEmitter::createFunction(FuncDeclaration& funcDecl)
{
    assert(!funcDecl.csym);
    IRState* irState = getCurrentIRState();
    bool nested = false;
    bool member = false;
    Dsymbol* parent = funcDecl.parent;

    AggregateDeclaration* aggrDecl = handleNested(*this, funcDecl, nested, member);
    if (!aggrDecl)
    {
        aggrDecl = DEREF(irState).hasAggregateDecl();
    }
    if (aggrDecl)
    {
        Aggregate& a = getAggregate(*aggrDecl);
        Method* m = createMethod(a.getBlock(), funcDecl, nested, parent);
        if (nested)
        {
            m->setNestedMember(member);
            m->generateBody();
        }
    }
    else if (!handleTemplateMethod(*this, funcDecl))
    {
        // All non-member functions are created at module scope, regardless of
        // the scope of their declaration in the source
        Method* method = createMethod(getBlock(), funcDecl);
        if (funcDecl.isStaticConstructor())
        {
            assembly_.addModuleStaticCtor(DEREF(method));
        }
    }
}


// Prepare block for static initializer
static void prepStaticInit(IRState* irState, IRState& initState, VarDeclaration& decl)
{
    if (AggregateDeclaration* aggrDecl = irState->hasAggregateDecl())
    {
        if (Class* c = DEREF(aggrDecl->csym).isClass())
        {
            // static members are initialized in a separate block
            // (which is eventually merged into the class' .cctor)
            initState.setBlock(c->getStaticInitBlock());
        }
        else if (Struct* s = DEREF(aggrDecl->csym).isStruct())
        {
            initState.setBlock(s->getStaticInitBlock());
        }
        else 
        {
            BackEnd::fatalError(decl.loc, "unhandled static initializer");
        }
    }
}


Field* ModuleEmitter::createGlobalVar(VarDeclaration& decl, IRState* irState)
{
    if (!irState)
    {
        irState = getCurrentIRState();
    }
    // put it in the static fields block
    block& blk = getStaticFields();

    // prevent duplicate names
    decl.ident = Identifier::generateId(decl.toChars(), blk.size());
    Field* field = Field::create(decl, true);

    string id = getDataSectionName() + "::'" + decl.toChars() + '\'';
    field->setName(Lexer::idPool(id.c_str()));
    
    blk.add(*field);

    if (hasTLS(decl))
    {
        blk.setHasTls();
    }
    decl.csym = field;
    TYPE& ilType = toILType(decl.loc, decl.type);
    // force initialization
    genInitializer(DEREF(irState), decl, ilType);
    return field;
}


void ModuleEmitter::createVar(VarDeclaration& decl)
{
    //IF_DEBUG(clog << __FUNCSIG__ << ": "<< decl.toChars() << endl);
    // do not expect the decl to have a backend symbol associated with it
    assert(decl.csym == NULL);

    IRState* irState = getCurrentIRState();
    // variable is at module scope, or function-scope static?
    if ((irState == irState_) ||
        (irState->hasFuncDecl() && (decl.storage_class & STCstatic))
        )
    {
        createGlobalVar(decl, irState);
    }
    else
    {   //create it inside function, or aggregate
        irState->createVar(decl);
        //make a temp state for the initializer
        IRState initState(DEREF(irState).getBlock());
        //initialize static fields inside the aggregate's static init block
        if (decl.storage_class & STCstatic)
        {
            prepStaticInit(irState, initState, decl);
        }

        if (decl.init)
        {
            initState.setDecl(&decl);

#if !VALUETYPE_STRUCT
            TYPE& type = toILType(decl);
            if (StructType* sType = type.isStructType())
            {   //make sure that the struct is not NULL
                BackEnd::construct(decl.loc, *sType);
            }
#endif
            auto_ptr<dt_t> (decl.init->toDt());
        }
        else
        {
            TYPE& t = toILType(decl.loc, decl.type);
            // give static arrays a chance to initialize
            if (t.isArrayType())
            {
                t.initVar(initState, decl, NULL, NULL);
            }
        }
    }
}


void ModuleEmitter::createVar(TypeInfoDeclaration& decl)
{
    Variable* v = loadTypeInfo(decl);
    getStaticInitBlock().add(*v);
    Field* fld = createGlobalVar(decl);
    fld->setTypeInfo();
    fld->bind(v);
}


Class* ModuleEmitter::createClass(ClassDeclaration& decl)
{
    Class* c = NULL;
    if (Dsymbol* parent = decl.toParent())
    {
        //start by assuming class is declared at module scope
        block* blk = &DEREF(irState_).getBlock();
        if (AggregateDeclaration* aggrDecl = parent->isAggregateDeclaration())
        {
            decl.isnested = true;
            blk = &getAggregate(*aggrDecl).getBlock();
        }

        //if the class lives in another assembly, generate it in a hidden block
#if 0
        for (Dsymbol* p = parent; p; p = p->toParent())
        {
            if (PragmaScope* pscope = p->isPragmaScope())
            {
                if (pscope->kind() == PragmaScope::pragma_assembly)
                {
                    blk = &BackEnd::instance().getAssembly();
                    break;
                }
            }
        }
#else
        if (inPragmaAssembly(parent))
        {
            blk = &BackEnd::instance().getAssembly();
        }
#endif
        c = new Class(decl, blk->depth());
        blk->add(*c);
        assert(decl.csym == c);
    }
    return c;
}



void ModuleEmitter::createStruct(StructDeclaration& decl)
{
    block& block = DEREF(irState_).getBlock();
    Struct* s = new Struct(decl, block.depth());
    block.add(*s);
    assert(decl.csym == s);
}


static void loadVariable(auto_ptr<dt_t>& dt, block& blk, Loc& loc)
{
    if (dt.get())
    {
        if (elem* e = dt->getElem())
        {
            if (Variable* v = e->isVariable())
            {
                bool loadAddr = (dt->op() == TOKsymoff);
                v->load(blk, &loc, loadAddr);
            }
        }
    }
}


void ModuleEmitter::genInitializer(IRState& state, VarDeclaration& decl, TYPE& type)
{
    block& blk = getStaticInitBlock();
    IRState initState(blk);
    initState.setDecl(&decl);
    if (Initializer* init = decl.init)
    {
        decl.init = NULL;
        auto_ptr<dt_t> dt(init->toDt());

        if (Variable* v = DEREF(decl.csym).isVariable())
        {
            if (v->isStatic() && !type.isArrayType())
            {
                loadVariable(dt, initState.getBlock(), decl.loc);
                v->store(initState.getBlock(), &decl.loc);
            }
        }
    }
    else
    {
        // Default-initialize if not explicitly initialized.
        // The front-end always generates default initializers for locals,
        // but not for module-scope variables, perhaps because it assumes
        // that in native executables the globals get zeroed out at load time.
        type.initVar(initState, decl, NULL, NULL);
    }
}



void ModuleEmitter::emitIL(wostream& out)
{
    time_t now = time(NULL);
    out << "//--------------------------------------------------------------\n";
    out << "// " << getModule().arg << " compiled: " << ctime(&now);
    out << "//--------------------------------------------------------------\n";

    if (emitAssembly_)
    {
        assembly_.emitIL(out); // emit assembly "header"

        if (!global.params.multiobj)
        {
            emitAssembly_ = false;
        }
    }
    out << ".module '" << getModule().toChars() << "'\n";
    if (!verifiable_)
    {
        out << ".custom instance void [mscorlib]System.Security"
               ".UnverifiableCodeAttribute::.ctor() = ( 01 00 00 00 )\n";
    }
    out << '\n';
    
    if (tls_)
    {
        tls_->emitIL(out);
    }
    emitStaticInit(out);
    DEREF(irState_).getBlock().emitIL(out);
}



/// Emit a class with .cctor that contains all module static data.
/// By qualifying the class by the D module name we avoid clashes with
/// other D modules.
void ModuleEmitter::emitStaticInit(wostream& out)
{
    if (staticInitBlock_ || staticFields_)
    {
        out << "//--------------------------------------------------------------\n";
        out << "// D module data\n";
        out << "//--------------------------------------------------------------\n";

        out << ".class public ";
        if (getStaticFields().hasTls() || getStaticInitBlock().hasTls())
        {
            out << "beforefieldinit ";
        }
        out << getDataSectionName().c_str() << "\n{\n";
        if (getStaticFields().emitIL(out))
        {
            out << '\n';
        }
        Indentable::indentOnce(out) << ".method static private void .cctor()\n";
        Indentable::indentOnce(out) << "{\n";
        staticInitBlock_->emitIL(out); // emit initializers

        Indentable::indentOnce(out);
        Indentable::indentOnce(out) << "ret\n";
        Indentable::indentOnce(out) << "}\n";

        out << "} // end of $data\n"; 
    }
}


void ModuleEmitter::emitIL(const char* filename)
{
    wofstream f(filename, ios_base::app);
    if (!f)
    {
        throw runtime_error(string("could not open file ") + filename);
    }
    getBlock().generateMethods();

    if (global.errors == 0)
    {
#ifdef DEBUG_TO_SCREEN
        wostringstream buf;
        emitIL(buf);
        f << buf.str();
        wclog << buf.str();
        wclog.flush();
#else
        emitIL(f);
#endif
    }
}


FuncDeclaration* ModuleEmitter::getCurrentFuncDeclaration() const
{
    FuncDeclaration* decl = NULL;
    for (vector<IRState*>::const_reverse_iterator ri = state_.rbegin(); 
         ri != state_.rend();
         ++ri)
    {
        decl = (*ri)->hasFuncDecl();
        if (decl)
        {
            break;
        }
    }
    return decl;
}


Method& getMethod(Loc& loc, FuncDeclaration& decl)
{
    Method* method = NULL;
    if (decl.csym)
    {
        method = decl.csym->isMethod();
    }
    if (!method)
    {
        BackEnd::fatalError(loc, "no method attached to function decl");
    }
    return *method;
}


Method& ModuleEmitter::getCurrentMethod(Loc& loc) const
{
    FuncDeclaration* decl = getCurrentFuncDeclaration();
    if (!decl)
    {
        BackEnd::fatalError(loc, "not in a function declaration");
    }
    return getMethod(loc, *decl);
}


void ModuleEmitter::setUnverifiable(Loc& loc)
{
    if (verifiable_)
    {
        //WARNING(loc, "unverifiable code");
        warning("%s: unverifiable code", loc.toChars());
    }
    verifiable_ = false;
    g_haveUnverifiableCode = true;
}


#if TLS_DATA_SLOT

block& ModuleEmitter::getTlsBlock(Loc& loc)
{
    if (!tls_)
    {
        tls_ = assembly_.addNewBlock();
        setUnverifiable(loc);
    }
    return *tls_;
}


namespace {
    ///<summary>
    /// Emit TLS data declaration
    ///</summary
    class TlsData : public dt_t
    {
        elem* emitILAsm(wostream& out)
        {
            Field* f = DEREF(getElem()).isField();
            indent(out) << ".data tls ";
            if (const char* label = getDataLabel())
            {
                out << label << " = ";
            }
            if (f->getType()->isfloating())
            {
                out << "float64\n";
            }
            else
            {
                out << "int64\n";
            }
            return this;
        }

    public:
        TlsData(Field& f, const char* label) : dt_t(&f, label)
        {
        }
    };
}


dt_t* ModuleEmitter::createTls(Field& f)
{
    block& tls = getTlsBlock(f.getDecl().loc);
    Identifier* id = Identifier::generateId("$data_", tls.size());
    dt_t* dt = new TlsData(f, id->toChars());
    tls.add(*dt);
    return dt;
}
#endif


bool ModuleEmitter::add(DefineDirective& def)
{
    if (defines_.insert(def).second)
    {
        DEREF(typedefs_).add(def);
        return true;
    }
    return false;
}
