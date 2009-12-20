#pragma once
//
// $Id: pragma.h 24625 2009-07-31 01:05:55Z unknown $
// Copyright (c) 2009 Cristian L. Vlasceanu
//
#include "dsymbol.h"

struct PragmaDeclaration;
struct StringExp;


struct PragmaScope : public ScopeDsymbol
{
    enum Kind
    {
        pragma_assembly,
    } whatKind;

    Module* module;

    PragmaScope(PragmaDeclaration*, Dsymbol*, StringExp*);
    Kind kind() const { return whatKind; }
    void toObjFile(int multiobj);
    void semantic(Scope*);    
    void setScope(Scope*);
    PragmaScope* isPragmaScope() { return this; }
};


PragmaScope* inPragmaAssembly(Dsymbol*);
