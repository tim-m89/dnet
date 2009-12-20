# vim: tabstop=4:softtabstop=4:noexpandtab:shiftwidth=4

PROGRAM=dnet
TEST=backend.net/smoketest
TARGETS=

SHELL=/bin/bash
CC=g++
SRCS=$(FRONTEND) $(BACKEND)
OBJS=$(FRONTEND:%.c=%.o) $(BACKEND:%.cpp=%.o)
#uncomment to force 32-bit target
#MACHINE=-m32

DEBUG=-g -DDEBUG 
CFLAGS=-D_DH -DTARGET_NET -DTARGET_WINDOS -DVALUETYPE_STRUCT $(MACHINE) $(DEBUG)
CXXFLAGS=$(CFLAGS) -I.
LDFLAGS=$(LLVM_LIBS)

FRONTEND=\
	access.c \
	aliasthis.c \
	array.c \
	arrayop.c \
	async.c \
	attrib.c \
	builtin.c \
	cast.c \
	class.c \
	clone.c \
	cond.c \
	constfold.c \
	dchar.c \
	declaration.c \
	delegatize.c \
	doc.c \
	dsymbol.c \
	dump.c \
	entity.c \
	enum.c \
	expression.c \
	func.c \
	funcvargs.c \
	gnuc.c \
	hdrgen.c \
	html.c \
	id.c \
	impcnvtab.c \
	identifier.c \
	import.c \
	inifile.c \
	init.c \
	inline.c \
	interpret.c \
	lexer.c \
	lstring.c \
	macro.c \
	man.c \
	mangle.c \
	mars.c \
	mem.c \
	module.c \
	mtype.c \
	opover.c \
	optimize.c \
	parse.c \
	port.c \
	response.c \
	root.c \
	scope.c \
	statement.c \
	staticassert.c \
	stringtable.c \
	struct.c \
	template.c \
	traits.c \
	typinf.c \
	unialpha.c \
	utf.c \
	json.c \
	version.c \
	frontend.net/externassembly.cpp \
	frontend.net/pragma.cpp \
	$(END)

MSIL=\
	backend.net/msil/aggr.cpp \
	backend.net/msil/arraylen.cpp \
	backend.net/msil/assembly.cpp \
	backend.net/msil/assocarray.cpp \
	backend.net/msil/assocarrayutil.cpp \
	backend.net/msil/arraytype.cpp \
	backend.net/msil/classemit.cpp \
	backend.net/msil/const.cpp \
	backend.net/msil/delegatetype.cpp \
	backend.net/msil/deref.cpp \
	backend.net/msil/enumemit.cpp \
	backend.net/msil/except.cpp \
	backend.net/msil/instruction.cpp \
	backend.net/msil/label.cpp \
	backend.net/msil/member.cpp \
	backend.net/msil/slice.cpp \
	backend.net/msil/structemit.cpp \
	backend.net/msil/typedef.cpp \
	backend.net/msil/types.cpp \
	backend.net/msil/variable.cpp \
	backend.net/msil/varargs.cpp \
	$(END)

BACKEND=\
	backend.net/asm.cpp \
	backend.net/backend.cpp \
	backend.net/block.cpp \
	backend.net/modemit.cpp \
	backend.net/cv.cpp \
	backend.net/decl.cpp \
	backend.net/elem.cpp \
	backend.net/dt.cpp \
	backend.net/expr.cpp \
	backend.net/hiddenmethod.cpp \
	backend.net/indentable.cpp \
	backend.net/irstate.cpp \
	backend.net/irstatecall.cpp \
	backend.net/library.cpp \
	backend.net/link.cpp \
	backend.net/objfile.cpp \
	backend.net/partialresult.cpp \
	backend.net/symbol.cpp \
	backend.net/test.cpp \
	backend.net/toir.cpp \
	backend.net/type.cpp \
	$(MSIL)


all: $(PROGRAM) $(TEST)
	$(TEST)

$(PROGRAM): id.h $(OBJS)
	$(CXX) $(MACHINE) $(OBJS) $(LDFLAGS) -o $@

TEST_SRCS =\
	array.c \
	mem.c \
	root.c \
	backend.net/smoketest.cpp \
	backend.net/test.cpp

$(TEST): $(TEST_SRCS)
	rm -f $@
	$(CXX) $(TEST_SRCS) $(CXXFLAGS) -o $@
	$(TEST)

id.h: idgen
	./idgen

id.c: idgen
	./idgen

impcnvtab.c: impcnvgen
	./impcnvgen 

idgen: idgen.c
	$(CC) $(CFLAGS) $< -o $@

impcnvgen: impcnvgen.c
	$(CC) $(CFLAGS) $< -o $@

clean:
	find . -name "*.o" -exec rm {} \;
	rm -f depend.mak $(PROGRAM) $(TEST) idgen id.h id.c impcnvgen impcnvtab.c

depend: id.h impcnvtab.c
	$(CC) $(CXXFLAGS) -MM $(FRONTEND) -MM > depend.mak
	for i in $(BACKEND); do $(CXX) $(CXXFLAGS) -MM $$i -MT $${i/.cpp/.o}; done >> depend.mak

-include depend.mak
