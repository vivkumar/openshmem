#include <stdio.h>               /* NULL                           */
#include <string.h>              /* memcpy()                       */
#include <sys/types.h>           /* size_t                         */

#include "gasnet_safe.h"         /* call wrapper w/ err handler    */

#include "state.h"
#include "symmem.h"
#include "comms.h"
#include "stats.h"
#include "updown.h"
#include "warn.h"

/*
 * short-circuit local puts/gets, otherwise translate between
 * local/remote addresses
 * (should probably ifdef for aligned segments case)
 */

/*
 * TODO: provide address translation routine (note repeated offset code below)
 */

#define SHMEM_TYPE_PUT(Name, Type)					\
  void									\
  shmem_##Name##_put(Type *dest, const Type *src, size_t len, int pe)	\
  {									\
    int typed_len = sizeof(Type) * len;					\
    if (__state.mype == pe) {						\
      memcpy(dest, src, typed_len);					\
    }									\
    else {								\
      size_t offset = (Type *)dest - (Type *)__symmetric_var_base(__state.mype); \
      void *rdest = (Type *)__symmetric_var_base(pe) + offset;		\
      if (! __symmetric_var_in_range(rdest, pe)) {			\
	__shmem_warn(SHMEM_LOG_FATAL,					\
		     "during shmem_%s_put() to PE %d, address %p not symmetric", \
		     #Name,						\
		     pe,						\
		     rdest						\
		     );							\
      }									\
      gasnet_put(pe, rdest, (Type *)src, typed_len);			\
    }									\
    SHMEM_STATS_PUT(pe);						\
  }

SHMEM_TYPE_PUT(short, short)
SHMEM_TYPE_PUT(int, int)
SHMEM_TYPE_PUT(long, long)
SHMEM_TYPE_PUT(longlong, long long)
SHMEM_TYPE_PUT(longdouble, long double)
SHMEM_TYPE_PUT(double, double)
SHMEM_TYPE_PUT(float, float)

_Pragma("weak shmem_putmem=shmem_long_put")
_Pragma("weak shmem_put=shmem_long_put")

  
#define SHMEM_TYPE_GET(Name, Type)					\
  void									\
  shmem_##Name##_get(Type *dest, const Type *src, size_t len, int pe) \
  {									\
    int typed_len = sizeof(Type) * len;					\
    if (__state.mype == pe) {						\
      memcpy(dest, src, typed_len);					\
    }									\
    else {								\
      size_t offset = (Type *)src - (Type *)__symmetric_var_base(__state.mype); \
      void *their_src = (Type *)__symmetric_var_base(pe) + offset;	\
      if (! __symmetric_var_in_range(their_src, pe)) {			\
	__shmem_warn(SHMEM_LOG_FATAL,					\
		     "during shmem_%s_get() from PE %d, address %p not symmetric", \
		     #Name,						\
		     pe,						\
		     their_src						\
		     );							\
      }									\
      gasnet_get(dest, pe, their_src, typed_len);			\
    }									\
    SHMEM_STATS_GET(pe);						\
  }

SHMEM_TYPE_GET(short, short)
SHMEM_TYPE_GET(int, int)
SHMEM_TYPE_GET(long, long)
SHMEM_TYPE_GET(longdouble, long double)
SHMEM_TYPE_GET(longlong, long long)
SHMEM_TYPE_GET(double, double)
SHMEM_TYPE_GET(float, float)

_Pragma("weak shmem_getmem=shmem_long_get")
_Pragma("weak shmem_get=shmem_long_get")

/*
 * gasnet_(put|get)_val can't handle bigger types..
 */

#define SHMEM_TYPE_P_WRAPPER(Name, Type)				\
  __inline__ void							\
  shmem_##Name##_p(Type *dest, Type value, int pe)			\
  {									\
    shmem_##Name##_put(dest, &value, sizeof(value), pe);		\
  }

SHMEM_TYPE_P_WRAPPER(float, float)
SHMEM_TYPE_P_WRAPPER(double, double)
SHMEM_TYPE_P_WRAPPER(longdouble, long double)
SHMEM_TYPE_P_WRAPPER(longlong, long long)

#define SHMEM_TYPE_P(Name, Type)					\
  void									\
  shmem_##Name##_p(Type *dest, Type value, int pe)			\
  {									\
    if (__state.mype == pe) {						\
      *dest = value;							\
    }									\
    else {								\
      int typed_len = sizeof(Type);					\
      size_t offset = (Type *)dest - (Type *)__symmetric_var_base(__state.mype); \
      void *rdest = (Type *)__symmetric_var_base(pe) + offset;		\
      if (! __symmetric_var_in_range(rdest, pe)) {			\
	__shmem_warn(SHMEM_LOG_FATAL,					\
		     "during shmem_%s_p() to PE %d, address %p not symmetric", \
		     #Name,						\
		     pe,						\
		     rdest						\
		     );							\
      }									\
      gasnet_put_val(pe, rdest, value, typed_len);			\
    }									\
    SHMEM_STATS_PUT(pe);						\
  }

SHMEM_TYPE_P(short, short)
SHMEM_TYPE_P(int, int)
SHMEM_TYPE_P(long, long)

#define SHMEM_TYPE_G_WRAPPER(Name, Type)				\
  __inline__ void							\
  shmem_##Name##_g(Type *dest, Type value, int pe)			\
  {									\
    shmem_##Name##_get(dest, &value, sizeof(value), pe);		\
  }

SHMEM_TYPE_G_WRAPPER(float, float)
SHMEM_TYPE_G_WRAPPER(double, double)
SHMEM_TYPE_G_WRAPPER(longlong, long long)
SHMEM_TYPE_G_WRAPPER(longdouble, long double)

#define SHMEM_TYPE_G(Name, Type)					\
  Type									\
  shmem_##Name##_g(Type *src, int pe)					\
  {									\
    Type retval;							\
    if (__state.mype == pe) {						\
      retval = *src;							\
    }									\
    else {								\
      int typed_len = sizeof(Type);					\
      size_t offset = (Type *)src - (Type *)__symmetric_var_base(__state.mype); \
      void *their_src = (Type *)__symmetric_var_base(pe) + offset;	\
      if (! __symmetric_var_in_range(their_src, pe)) {			\
	__shmem_warn(SHMEM_LOG_FATAL,					\
		     "during shmem_%s_g() from PE %d, address %p not symmetric", \
		     #Name,						\
		     pe,						\
		     their_src						\
		     );							\
      }									\
      retval = (Type) gasnet_get_val(pe, their_src, typed_len);		\
    }									\
    SHMEM_STATS_GET(pe);						\
    return retval;							\
  }

SHMEM_TYPE_G(short, short)
SHMEM_TYPE_G(int, int)
SHMEM_TYPE_G(long, long)
