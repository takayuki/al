#ifndef _TM_H
#define _TM_H
#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#include <assert.h>

#define MAIN(argc,argv)			int main (int argc, char** argv)
#define MAIN_RETURN(val)		return val
#define GOTO_SIM()
#define GOTO_REAL()
#define IS_IN_SIM()			(0)
#define SIM_GET_NUM_CPU(var)
#define TM_PRINTF			printf
#define TM_PRINT0			printf
#define TM_PRINT1			printf
#define TM_PRINT2			printf
#define TM_PRINT3			printf
#define P_MEMORY_STARTUP(num)
#define P_MEMORY_SHUTDOWN()
#define TM_STARTUP(num)
#define TM_SHUTDOWN()
#define TM_THREAD_ENTER()
#define TM_THREAD_EXIT()
#define P_MALLOC(size)			tmalloc_reserve(size)
#define P_FREE(ptr)			tmalloc_release(ptr)
#define TM_MALLOC(size)			malloc(size)
#define TM_FREE(ptr)			free(ptr)
#define TM_BEGIN()
#define TM_BEGIN_RO()
#define TM_END()
#define TM_RESTART()			assert(0)
#define TM_EARLY_RELEASE(var)
#define TM_SHARED_READ(var)		(var)
#define TM_SHARED_READ_P(var)		(var)
#define TM_SHARED_READ_F(var)		(var)
#define TM_SHARED_WRITE(var,val)	({var = val; var;})
#define TM_SHARED_WRITE_P(var,val)	({var = val; var;})
#define TM_SHARED_WRITE_F(var,val)	({var = val; var;})
#define TM_LOCAL_WRITE(var,val)		(LocalStore(&(var),val))
#define TM_LOCAL_WRITE_P(var,val)	(LocalStore(&(var),val))
#define TM_LOCAL_WRITE_F(var,val)	(LocalStoreF(&(var),val))

#endif
