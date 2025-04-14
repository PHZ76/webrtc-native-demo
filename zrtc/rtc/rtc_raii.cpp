#include "rtc_log.h"
#include "srtp.h"

static void init_libsrtp()
{
	srtp_err_status_t status = srtp_init();
	if (status != srtp_err_status_ok) {
		RTC_LOG_ERROR("srtp_init failed, status:{}", (int)status);
	}
	else {
		RTC_LOG_INFO("[raii] srtp_init succeed, version:{}", srtp_get_version());
	}
}

class RTC_RAII
{
public:
	RTC_RAII()
	{
		init_rtc_log();
		RTC_LOG_INFO("[raii] init log.");

		init_libsrtp();
	}

	~RTC_RAII()
	{
		
	}
};

static RTC_RAII rtc_raii;