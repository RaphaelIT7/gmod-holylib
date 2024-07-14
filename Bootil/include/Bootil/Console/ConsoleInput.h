#pragma once

namespace Bootil
{
	namespace Console
	{
		namespace Input
		{
			BString GetLine();
			BString GetLineInProgress();

			// Flush all input etc
			void Flush();

			void PreOutput();
			void PostOutput();
		};
	}
}