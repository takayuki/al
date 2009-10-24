#ifndef _STM_H
#define _STM_H

#include <tl2/tl2.h>
#include <pthread.h>

#define STM_NEW_THREAD()	TxNewThread()
#define STM_INIT_THREAD(t)	TxInitThread((t),(long)pthread_self())
#define STM_FREE_THREAD		TxFreeThread
#define STM_BEGIN(t,buf,ro)	TxStart((t),&(buf),&(ro));
#define STM_COMMIT(t)		TxCommit((t))
#define STM_LOAD(t,var)		TxLoad(t,(intptr_t*)&(var))
#define STM_STORE(t,var,val)	TxStore(t,(intptr_t*)&(var),((intptr_t)(val)))

#endif
