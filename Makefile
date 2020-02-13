# Copyright (C) 2019 Trevor Last
# See LICENSE file for copyright and license details.

CXXFLAGS=-Wall -Wextra -g
LDFLAGS=-lSDL2


SRCDIR=src
OBJDIR=$(SRCDIR)/obj
DEPDIR=$(SRCDIR)/dep


SRC=$(wildcard $(SRCDIR)/*.cpp)
OBJ=$(subst $(SRCDIR),$(OBJDIR),$(SRC:.cpp=.o))
DEP=$(subst $(SRCDIR),$(DEPDIR),$(SRC:.cpp=.d))

FONTS=$(addprefix font/,\
	80col-normal.pbm \
	80col-doublewidth.pbm \
	80col-doubleheight.pbm \
	132col-normal.pbm \
	132col-doublewidth.pbm \
	132col-doubleheight.pbm)


all : term $(FONTS)

term : $(OBJ)
	$(CXX) $^ $(CXXFLAGS) $(LDFLAGS) -o $@

buildfont : font/mkfont/mkfont.cpp src/obj/loadfont.o
	$(CXX) $^ $(CXXFLAGS) -o $@

$(FONTS) : buildfont font/mkfont/vt100font-source.pbm
	@echo "Building fonts..."
	@./buildfont font/mkfont/vt100font-source.pbm
	@mv $(notdir $(FONTS)) font/
	@echo "Done."

include $(DEP)


$(OBJDIR)/%.o : $(SRCDIR)/%.cpp
	$(CXX) -c $< $(CXXFLAGS) $(LDFLAGS) -o $@

$(DEPDIR)/%.d : $(SRCDIR)/%.cpp
	$(CXX) $^ $(CXXFLAGS) $(LDFLAGS) -MM -MT $(subst $(DEPDIR),$(OBJDIR),$(@:.d=.o)) -MF $@

$(OBJ) :|$(OBJDIR)
$(DEP) :|$(DEPDIR)

$(OBJDIR) :
	@mkdir $@
$(DEPDIR) :
	@mkdir $@



.PHONY: clean
clean:
	@rm -f term buildfont $(OBJDIR)/* $(DEPDIR)/* $(FONTS)


