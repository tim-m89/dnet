
// Compiler implementation of the D programming language
// Copyright (c) 1999-2008 by Digital Mars
// All Rights Reserved
// written by Walter Bright
// http://www.digitalmars.com
// License for redistribution is by either the Artistic License
// in artistic.txt, or the GNU General Public License in gnu.txt.
// See the included readme.txt for details.

#ifndef DMD_AGGREGATE_H
#define DMD_AGGREGATE_H

#ifdef __DMC__
#pragma once
#endif /* __DMC__ */

#include "root.h"
#include "dsymbol.h"

struct Identifier;
struct Type;
struct TypeFunction;
struct Expression;
struct FuncDeclaration;
struct CtorDeclaration;
struct DtorDeclaration;
struct InvariantDeclaration;
struct NewDeclaration;
struct DeleteDeclaration;
struct InterfaceDeclaration;
struct ClassInfoDeclaration;
struct VarDeclaration;
struct dt_t;


struct AggregateDeclaration : ScopeDsymbol
{
    Type *type;
    unsigned storage_class;
    enum PROT protection;
    Type *handle;		// 'this' type
    unsigned structsize;	// size of struct
    unsigned alignsize;		// size of struct for alignment purposes
    unsigned structalign;	// struct member alignment in effect
    int hasUnions;		// set if aggregate has overlapping fields
    Array fields;		// VarDeclaration fields
    unsigned sizeok;		// set when structsize contains valid data
				// 0: no size
				// 1: size is correct
				// 2: cannot determine size; fwd referenced
    int isdeprecated;		// !=0 if deprecated

    int isnested;		// !=0 if is nested
    VarDeclaration *vthis;	// 'this' parameter if this aggregate is nested

    // Special member functions
    InvariantDeclaration *inv;		// invariant
    NewDeclaration *aggNew;		// allocator
    DeleteDeclaration *aggDelete;	// deallocator

#if DMDV2
    //CtorDeclaration *ctor;
    Dsymbol *ctor;			// CtorDeclaration or TemplateDeclaration
    CtorDeclaration *defaultCtor;	// default constructor
    Dsymbol *aliasthis;			// forward unresolved lookups to aliasthis
#endif

    FuncDeclarations dtors;	// Array of destructors
    FuncDeclaration *dtor;	// aggregate destructor

#ifdef IN_GCC
    Array methods;              // flat list of all methods for debug information
#endif

    AggregateDeclaration(Loc loc, Identifier *id);
    void semantic2(Scope *sc);
    void semantic3(Scope *sc);
    void inlineScan();
    unsigned size(Loc loc);
    static void alignmember(unsigned salign, unsigned size, unsigned *poffset);
    Type *getType();
    void addField(Scope *sc, VarDeclaration *v);
    int isDeprecated();		// is aggregate deprecated?
    FuncDeclaration *buildDtor(Scope *sc);
    int isNested();

    void emitComment(Scope *sc);
    void toJsonBuffer(OutBuffer *buf);
    void toDocBuffer(OutBuffer *buf);

    // For access checking
    virtual PROT getAccess(Dsymbol *smember);	// determine access to smember
    int isFriendOf(AggregateDeclaration *cd);
    int hasPrivateAccess(Dsymbol *smember);	// does smember have private access to members of this class?
    void accessCheck(Loc loc, Scope *sc, Dsymbol *smember);

    enum PROT prot();

    // Back end
    Symbol *stag;		// tag symbol for debug data
    Symbol *sinit;
    Symbol *toInitializer();

    AggregateDeclaration *isAggregateDeclaration() { return this; }
};

struct AnonymousAggregateDeclaration : AggregateDeclaration
{
    AnonymousAggregateDeclaration()
	: AggregateDeclaration(0, NULL)
    {
    }

    AnonymousAggregateDeclaration *isAnonymousAggregateDeclaration() { return this; }
};

struct StructDeclaration : AggregateDeclaration
{
    int zeroInit;		// !=0 if initialize with 0 fill
#if DMDV2
    int hasIdentityAssign;	// !=0 if has identity opAssign
    FuncDeclaration *cpctor;	// generated copy-constructor, if any

    FuncDeclarations postblits;	// Array of postblit functions
    FuncDeclaration *postblit;	// aggregate postblit
#endif

    StructDeclaration(Loc loc, Identifier *id);
    Dsymbol *syntaxCopy(Dsymbol *s);
    void semantic(Scope *sc);
    void toCBuffer(OutBuffer *buf, HdrGenState *hgs);
    char *mangle();
    const char *kind();
    int needOpAssign();
    FuncDeclaration *buildOpAssign(Scope *sc);
    FuncDeclaration *buildPostBlit(Scope *sc);
    FuncDeclaration *buildCpCtor(Scope *sc);
    void toDocBuffer(OutBuffer *buf);

    PROT getAccess(Dsymbol *smember);	// determine access to smember

    void toObjFile(int multiobj);			// compile to .obj file
    void toDt(dt_t **pdt);
    void toDebug();			// to symbolic debug info

    StructDeclaration *isStructDeclaration() { return this; }
};

struct UnionDeclaration : StructDeclaration
{
    UnionDeclaration(Loc loc, Identifier *id);
    Dsymbol *syntaxCopy(Dsymbol *s);
    const char *kind();

    UnionDeclaration *isUnionDeclaration() { return this; }
};

struct BaseClass
{
    Type *type;				// (before semantic processing)
    enum PROT protection;		// protection for the base interface

    ClassDeclaration *base;
    int offset;				// 'this' pointer offset
    Array vtbl;				// for interfaces: Array of FuncDeclaration's
					// making up the vtbl[]

    int baseInterfaces_dim;
    BaseClass *baseInterfaces;		// if BaseClass is an interface, these
					// are a copy of the InterfaceDeclaration::interfaces

    BaseClass();
    BaseClass(Type *type, enum PROT protection);

    int fillVtbl(ClassDeclaration *cd, Array *vtbl, int newinstance);
    void copyBaseInterfaces(BaseClasses *);
};

#if DMDV2
#define CLASSINFO_SIZE 	(0x3C+16+4)	// value of ClassInfo.size
#else
#define CLASSINFO_SIZE 	(0x3C+12+4)	// value of ClassInfo.size
#endif

struct ClassDeclaration : AggregateDeclaration
{
    static ClassDeclaration *object;
    static ClassDeclaration *classinfo;

    ClassDeclaration *baseClass;	// NULL only if this is Object
    FuncDeclaration *staticCtor;
    FuncDeclaration *staticDtor;
    Array vtbl;				// Array of FuncDeclaration's making up the vtbl[]
    Array vtblFinal;			// More FuncDeclaration's that aren't in vtbl[]

    BaseClasses baseclasses;		// Array of BaseClass's; first is super,
					// rest are Interface's

    int interfaces_dim;
    BaseClass **interfaces;		// interfaces[interfaces_dim] for this class
					// (does not include baseClass)

    BaseClasses *vtblInterfaces;	// array of base interfaces that have
					// their own vtbl[]

    ClassInfoDeclaration *vclassinfo;	// the ClassInfo object for this ClassDeclaration
    int com;				// !=0 if this is a COM class (meaning
					// it derives from IUnknown)
    int isauto;				// !=0 if this is an auto class
    int isabstract;			// !=0 if abstract class

    int inuse;				// to prevent recursive attempts

    ClassDeclaration(Loc loc, Identifier *id, BaseClasses *baseclasses);
    Dsymbol *syntaxCopy(Dsymbol *s);
    void semantic(Scope *sc);
    void toCBuffer(OutBuffer *buf, HdrGenState *hgs);
    int isBaseOf2(ClassDeclaration *cd);

    #define OFFSET_RUNTIME 0x76543210
    virtual int isBaseOf(ClassDeclaration *cd, int *poffset);

    Dsymbol *search(Loc, Identifier *ident, int flags);
#if DMDV2
    int isFuncHidden(FuncDeclaration *fd);
#endif
    FuncDeclaration *findFunc(Identifier *ident, TypeFunction *tf);
    void interfaceSemantic(Scope *sc);
    int isCOMclass();
    virtual int isCOMinterface();
#if DMDV2
    virtual int isCPPinterface();
#endif
    int isAbstract();
    virtual int vtblOffset();
    const char *kind();
    char *mangle();
    void toDocBuffer(OutBuffer *buf);

    PROT getAccess(Dsymbol *smember);	// determine access to smember

    void addLocalClass(ClassDeclarations *);

    // Back end
    void toObjFile(int multiobj);			// compile to .obj file
    void toDebug();
    unsigned baseVtblOffset(BaseClass *bc);
    Symbol *toSymbol();
    Symbol *toVtblSymbol();
    void toDt(dt_t **pdt);
    void toDt2(dt_t **pdt, ClassDeclaration *cd);

    Symbol *vtblsym;

    ClassDeclaration *isClassDeclaration() { return (ClassDeclaration *)this; }
};

struct InterfaceDeclaration : ClassDeclaration
{
#if DMDV2
    int cpp;				// !=0 if this is a C++ interface
#endif
    InterfaceDeclaration(Loc loc, Identifier *id, BaseClasses *baseclasses);
    Dsymbol *syntaxCopy(Dsymbol *s);
    void semantic(Scope *sc);
    int isBaseOf(ClassDeclaration *cd, int *poffset);
    int isBaseOf(BaseClass *bc, int *poffset);
    const char *kind();
    int vtblOffset();
#if DMDV2
    int isCPPinterface();
#endif
    virtual int isCOMinterface();

    void toObjFile(int multiobj);			// compile to .obj file
    Symbol *toSymbol();

    InterfaceDeclaration *isInterfaceDeclaration() { return this; }
};

#endif /* DMD_AGGREGATE_H */
