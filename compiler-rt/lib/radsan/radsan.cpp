#include <radsan/radsan.h>
#include <radsan/radsan_context.h>
#include <radsan/radsan_interceptors.h>
#include <unistd.h>

extern "C" {
RADSAN_EXPORT void radsan_init()
{
    radsan::initialiseInterceptors();
}

RADSAN_EXPORT void radsan_realtime_enter()
{
    radsan::getContextForThisThread().realtimePush();
}

RADSAN_EXPORT void radsan_realtime_exit()
{
    radsan::getContextForThisThread().realtimePop();
}
}
