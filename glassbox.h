#ifndef GLASSBOX_H
#define GLASSBOX_H

#ifdef GLASSBOX
 #include <assert.h>
 #define HAS_GLASSBOX 1
 #define WHEN_GLASSBOX(x) x
#else
 #define HAS_GLASSBOX 0
 #define WHEN_GLASSBOX(x) ((void)0)
#endif

#define glass_assert(x) WHEN_GLASSBOX(assert(x))

#endif
