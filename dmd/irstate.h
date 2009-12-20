
// Compiler implementation of the D programming language
// Copyright (c) 1999-2008 by Digital Mars
// All Rights Reserved
// written by Walter Bright
// http://www.digitalmars.com

#ifndef DMD_CONTEXT_H
#define DMD_CONTEXT_H

#ifdef __DMC__
#pragma once
#endif /* __DMC__ */

struct Module;
struct Statement;
struct block;
struct Dsymbol;
struct Identifier;
struct Symbol;
struct FuncDeclaration;
struct Blockx;
struct Array;
struct elem;

struct IRState
{
    IRState *prev;
    Statement *statement;
    Module *m;			// module
    Dsymbol *symbol;
    Identifier *ident;
    Symbol *shidden;		// hidden parameter to function
    Symbol *sthis;		// 'this' parameter to function (member and nested)
    Symbol *sclosure;		// pointer to closure instance
    Blockx *blx;
    Array *deferToObj;		// array of Dsymbol's to run toObjFile(int multiobj) on later
    elem *ehidden;		// transmit hidden pointer to CallExp::toElem()
    Symbol *startaddress;

    block *breakBlock;
    block *contBlock;
    block *switchBlock;
    block *defaultBlock;

    IRState(IRState *irs, Statement *s);
    IRState(IRState *irs, Dsymbol *s);
    IRState(Module *m, Dsymbol *s);

    block *getBreakBlock(Identifier *ident);
    block *getContBlock(Identifier *ident);
    block *getSwitchBlock();
    block *getDefaultBlock();
    FuncDeclaration *getFunc();
};

#endif /* DMD_CONTEXT_H */
