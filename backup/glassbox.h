#ifndef GLASSBOX_H
#define GLASSBOX_H

#ident "Copyright (c) 2012-2013 Tokutek Inc.  All rights reserved."
#ident "$Id$"

#ifdef GLASSBOX
 #include <assert.h>
 #define HAS_GLASSBOX 1
 #define WHEN_GLASSBOX(x) x
  #define glass_assert(x) assert(x)
#else
 #define HAS_GLASSBOX 0
 #define WHEN_GLASSBOX(x) ((void)0)
 #define glass_assert(x) ignore(x)
#endif


#endif
