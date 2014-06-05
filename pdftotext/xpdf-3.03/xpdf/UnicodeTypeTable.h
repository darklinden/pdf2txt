//========================================================================
//
// UnicodeTypeTable.h
//
// Copyright 2003 Glyph & Cog, LLC
//
//========================================================================

#ifndef UNICODETYPETABLE_H
#define UNICODETYPETABLE_H

#include "../goo/gtypes.h"

extern GBool unicodeTypeL(Unicode c);

extern GBool unicodeTypeR(Unicode c);

extern GBool unicodeTypeNum(Unicode c);

extern GBool unicodeTypeAlphaNum(Unicode c);

extern Unicode unicodeToUpper(Unicode c);

#endif