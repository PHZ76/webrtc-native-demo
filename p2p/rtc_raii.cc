#include <windows.h>
#include <shellapi.h>  
#include "rtc_base/ssl_adapter.h"
#include "rtc_base/win32_socket_init.h"
#include "rtc_base/win32_socket_server.h"

class RTC_RAII
{
public:
	RTC_RAII()
	{
		rtc::InitializeSSL();
	}

	~RTC_RAII()
	{
		rtc::CleanupSSL();
	}
};

static RTC_RAII rtc_raii;
