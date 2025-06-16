#pragma once

namespace Bootil
{
	namespace Time
	{
		BOOTIL_EXPORT unsigned long long	UnixTimestamp();
		BOOTIL_EXPORT Bootil::BString		UnixTimestampAsString();

		BOOTIL_EXPORT Bootil::BString		TimeStamp(); // returns "23:45"
		BOOTIL_EXPORT Bootil::BString		TimeAndDateStamp(); // returns "1982-05-20 23:45"

		BOOTIL_EXPORT float Seconds();
		BOOTIL_EXPORT float MilliSeconds();

		class Timer
		{
			public:

				Timer()
				{
					Reset();
				}

				void Reset( float fTimeOffset = 0.0f )
				{
					m_TimeStarted = Bootil::Time::Seconds() - fTimeOffset;
				}

				float Seconds()
				{
					return Bootil::Time::Seconds() - m_TimeStarted;
				}

				float m_TimeStarted;
				float m_fScale;
		};
	}

}