
// Compiler implementation of the D programming language
// Copyright (c) 1999-2009 by Digital Mars
// All Rights Reserved
// written by Walter Bright
// http://www.digitalmars.com
// License for redistribution is by either the Artistic License
// in artistic.txt, or the GNU General Public License in gnu.txt.
// See the included readme.txt for details.

#ifndef DMD_DSYMBOL_H
#define DMD_DSYMBOL_H

#ifdef __DMC__
#pragma once
#endif /* __DMC__ */

#include "root.h"
#include "stringtable.h"

#include "mars.h"
#include "arraytypes.h"

struct Identifier;
struct Scope;
struct DsymbolTable;
struct Declaration;
struct ThisDeclaration;
struct TupleDeclaration;
struct TypedefDeclaration;
struct AliasDeclaration;
struct AggregateDeclaration;
struct EnumDeclaration;
struct ClassDeclaration;
struct InterfaceDeclaration;
struct StructDeclaration;
struct UnionDeclaration;
struct FuncDeclaration;
struct FuncAliasDeclaration;
struct FuncLiteralDeclaration;
struct CtorDeclaration;
struct PostBlitDeclaration;
struct DtorDeclaration;
struct StaticCtorDeclaration;
struct StaticDtorDeclaration;
struct InvariantDeclaration;
struct UnitTestDeclaration;
struct NewDeclaration;
struct VarDeclaration;
struct AttribDeclaration;
struct Symbol;
struct Package;
struct Module;
struct Import;
struct Type;
struct TypeTuple;
struct WithStatement;
struct LabelDsymbol;
struct ScopeDsymbol;
struct TemplateDeclaration;
struct TemplateInstance;
struct TemplateMixin;
struct EnumMember;
struct ScopeDsymbol;
struct WithScopeSymbol;
struct ArrayScopeSymbol;
struct SymbolDeclaration;
struct Expression;
struct DeleteDeclaration;
struct HdrGenState;
struct OverloadSet;
#if TARGET_NET
struct PragmaScope;
#endif
#if IN_GCC
union tree_node;
typedef union tree_node TYPE;
#else
struct TYPE;
#endif

// Back end
struct Classsym;

enum PROT
{
    PROTundefined,
    PROTnone,		// no access
    PROTprivate,
    PROTpackage,
    PROTprotected,
    PROTpublic,
    PROTexport,
};


struct Dsymbol : Object
{
    Identifier *ident;
    Identifier *c_ident;
    Dsymbol *parent;
    Symbol *csym;		// symbol for code generator
    Symbol *isym;		// import version of csym
    unsigned char *comment;	// documentation comment for this Dsymbol
    Loc loc;			// where defined
    Scope *scope;		// !=NULL means context to use for semantic()

    Dsymbol();
    Dsymbol(Identifier *);
    char *toChars();
    char *locToChars();
    int equals(Object *o);
    int isAnonymous();
    void error(Loc loc, const char *format, ...);
    void error(const char *format, ...);
    void checkDeprecated(Loc loc, Scope *sc);
    Module *getModule();
    Dsymbol *pastMixin();
    Dsymbol *toParent();
    Dsymbol *toParent2();
    TemplateInstance *inTemplateInstance();

    int dyncast() { return DYNCAST_DSYMBOL; }	// kludge for template.isSymbol()

    static Array *arraySyntaxCopy(Array *a);

    virtual const char *toPrettyChars();
    virtual const char *kind();
    virtual Dsymbol *toAlias();			// resolve real symbol
    virtual int addMember(Scope *sc, ScopeDsymbol *s, int memnum);
    virtual void setScope(Scope *sc);
    virtual void importAll(Scope *sc);
    virtual void semantic(Scope *sc);
    virtual void semantic2(Scope *sc);
    virtual void semantic3(Scope *sc);
    virtual void inlineScan();
    virtual Dsymbol *search(Loc loc, Identifier *ident, int flags);
    Dsymbol *searchX(Loc loc, Scope *sc, Identifier *id);
    virtual int overloadInsert(Dsymbol *s);
#ifdef _DH
    char *toHChars();
    virtual void toHBuffer(OutBuffer *buf, HdrGenState *hgs);
#endif
    virtual void toCBuffer(OutBuffer *buf, HdrGenState *hgs);
    virtual void toDocBuffer(OutBuffer *buf);
    virtual void toJsonBuffer(OutBuffer *buf);
    virtual unsigned size(Loc loc);
    virtual int isforwardRef();
    virtual void defineRef(Dsymbol *s);
    virtual AggregateDeclaration *isThis();	// is a 'this' required to access the member
    virtual ClassDeclaration *isClassMember();	// are we a member of a class?
    virtual int isExport();			// is Dsymbol exported?
    virtual int isImportedSymbol();		// is Dsymbol imported?
    virtual int isDeprecated();			// is Dsymbol deprecated?
#if DMDV2
    virtual int isOverloadable();
#endif
    virtual LabelDsymbol *isLabel();		// is this a LabelDsymbol?
    virtual AggregateDeclaration *isMember();	// is this symbol a member of an AggregateDeclaration?
    virtual Type *getType();			// is this a type?
    virtual char *mangle();
    virtual int needThis();			// need a 'this' pointer?
    virtual enum PROT prot();
    virtual Dsymbol *syntaxCopy(Dsymbol *s);	// copy only syntax trees
    virtual int oneMember(Dsymbol **ps);
    static int oneMembers(Array *members, Dsymbol **ps);
    virtual int hasPointers();
    virtual void addLocalClass(ClassDeclarations *) { }
    virtual void checkCtorConstInit() { }

    virtual void addComment(unsigned char *comment);
    virtual void emitComment(Scope *sc);
    void emitDitto(Scope *sc);

    // Backend

    virtual Symbol *toSymbol();			// to backend symbol
    virtual void toObjFile(int multiobj);			// compile to .obj file
    virtual int cvMember(unsigned char *p);	// emit cv debug info for member

    Symbol *toImport();				// to backend import symbol
    static Symbol *toImport(Symbol *s);		// to backend import symbol

    Symbol *toSymbolX(const char *prefix, int sclass, TYPE *t, const char *suffix);	// helper

    // Eliminate need for dynamic_cast
    virtual Package *isPackage() { return NULL; }
    virtual Module *isModule() { return NULL; }
    virtual EnumMember *isEnumMember() { return NULL; }
    virtual TemplateDeclaration *isTemplateDeclaration() { return NULL; }
    virtual TemplateInstance *isTemplateInstance() { return NULL; }
    virtual TemplateMixin *isTemplateMixin() { return NULL; }
    virtual Declaration *isDeclaration() { return NULL; }
    virtual ThisDeclaration *isThisDeclaration() { return NULL; }
    virtual TupleDeclaration *isTupleDeclaration() { return NULL; }
    virtual TypedefDeclaration *isTypedefDeclaration() { return NULL; }
    virtual AliasDeclaration *isAliasDeclaration() { return NULL; }
    virtual AggregateDeclaration *isAggregateDeclaration() { return NULL; }
    virtual FuncDeclaration *isFuncDeclaration() { return NULL; }
    virtual FuncAliasDeclaration *isFuncAliasDeclaration() { return NULL; }
    virtual FuncLiteralDeclaration *isFuncLiteralDeclaration() { return NULL; }
    virtual CtorDeclaration *isCtorDeclaration() { return NULL; }
    virtual PostBlitDeclaration *isPostBlitDeclaration() { return NULL; }
    virtual DtorDeclaration *isDtorDeclaration() { return NULL; }
    virtual StaticCtorDeclaration *isStaticCtorDeclaration() { return NULL; }
    virtual StaticDtorDeclaration *isStaticDtorDeclaration() { return NULL; }
    virtual InvariantDeclaration *isInvariantDeclaration() { return NULL; }
    virtual UnitTestDeclaration *isUnitTestDeclaration() { return NULL; }
    virtual NewDeclaration *isNewDeclaration() { return NULL; }
    virtual VarDeclaration *isVarDeclaration() { return NULL; }
    virtual ClassDeclaration *isClassDeclaration() { return NULL; }
    virtual StructDeclaration *isStructDeclaration() { return NULL; }
    virtual UnionDeclaration *isUnionDeclaration() { return NULL; }
    virtual InterfaceDeclaration *isInterfaceDeclaration() { return NULL; }
    virtual ScopeDsymbol *isScopeDsymbol() { return NULL; }
    virtual WithScopeSymbol *isWithScopeSymbol() { return NULL; }
    virtual ArrayScopeSymbol *isArrayScopeSymbol() { return NULL; }
    virtual Import *isImport() { return NULL; }
    virtual EnumDeclaration *isEnumDeclaration() { return NULL; }
#ifdef _DH
    virtual DeleteDeclaration *isDeleteDeclaration() { return NULL; }
#endif
    virtual SymbolDeclaration *isSymbolDeclaration() { return NULL; }
    virtual AttribDeclaration *isAttribDeclaration() { return NULL; }
    virtual OverloadSet *isOverloadSet() { return NULL; }
#if TARGET_NET
    virtual PragmaScope* isPragmaScope() { return NULL; }
#endif
};

// Dsymbol that generates a scope

struct ScopeDsymbol : Dsymbol
{
    Array *members;		// all Dsymbol's in this scope
    DsymbolTable *symtab;	// members[] sorted into table

    Array *imports;		// imported ScopeDsymbol's
    unsigned char *prots;	// array of PROT, one for each import

    ScopeDsymbol();
    ScopeDsymbol(Identifier *id);
    Dsymbol *syntaxCopy(Dsymbol *s);
    Dsymbol *search(Loc loc, Identifier *ident, int flags);
    void importScope(ScopeDsymbol *s, enum PROT protection);
    int isforwardRef();
    void defineRef(Dsymbol *s);
    static void multiplyDefined(Loc loc, Dsymbol *s1, Dsymbol *s2);
    Dsymbol *nameCollision(Dsymbol *s);
    const char *kind();
    FuncDeclaration *findGetMembers();
    virtual Dsymbol *symtabInsert(Dsymbol *s);

    void emitMemberComments(Scope *sc);

    static size_t dim(Array *members);
    static Dsymbol *getNth(Array *members, size_t nth, size_t *pn = NULL);

    ScopeDsymbol *isScopeDsymbol() { return this; }
};

// With statement scope

struct WithScopeSymbol : ScopeDsymbol
{
    WithStatement *withstate;

    WithScopeSymbol(WithStatement *withstate);
    Dsymbol *search(Loc loc, Identifier *ident, int flags);

    WithScopeSymbol *isWithScopeSymbol() { return this; }
};

// Array Index/Slice scope

struct ArrayScopeSymbol : ScopeDsymbol
{
    Expression *exp;	// IndexExp or SliceExp
    TypeTuple *type;	// for tuple[length]
    TupleDeclaration *td;	// for tuples of objects
    Scope *sc;

    ArrayScopeSymbol(Scope *sc, Expression *e);
    ArrayScopeSymbol(Scope *sc, TypeTuple *t);
    ArrayScopeSymbol(Scope *sc, TupleDeclaration *td);
    Dsymbol *search(Loc loc, Identifier *ident, int flags);

    ArrayScopeSymbol *isArrayScopeSymbol() { return this; }
};

// Overload Sets

#if DMDV2
struct OverloadSet : Dsymbol
{
    Dsymbols a;		// array of Dsymbols

    OverloadSet();
    void push(Dsymbol *s);
    OverloadSet *isOverloadSet() { return this; }
    const char *kind();
};
#endif

// Table of Dsymbol's

struct DsymbolTable : Object
{
    StringTable *tab;

    DsymbolTable();
    ~DsymbolTable();

    // Look up Identifier. Return Dsymbol if found, NULL if not.
    Dsymbol *lookup(Identifier *ident);

    // Insert Dsymbol in table. Return NULL if already there.
    Dsymbol *insert(Dsymbol *s);

    // Look for Dsymbol in table. If there, return it. If not, insert s and return that.
    Dsymbol *update(Dsymbol *s);
    Dsymbol *insert(Identifier *ident, Dsymbol *s);	// when ident and s are not the same
};

#endif /* DMD_DSYMBOL_H */
