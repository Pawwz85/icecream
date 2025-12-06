#include "client.h"
#include <stdarg.h>

std::string UCI::formatString(std::string format, ...)
{
	std::string result = "";
	int phase = 0;
	va_list args;
	va_start(args, format);
	std::string str;
	int32_t decimal;

	char buff[32];
	char* it;
	for (char c : format) {

		if (phase) goto PHASE_2;
		
		if (c == '%')
			phase = 1;
		else
			result.push_back(c);
		
		continue;

	PHASE_2:

		if (c == '%' || c == 'd' || c == 's') {
			// TODO: handle writing parameter to format

			switch (c) {
				case '%':
					result += "%";
					break;
				case 's':
					str = va_arg(args, const char*);
					result += str;
					break;
				case 'd':
					decimal = va_arg(args, int32_t);
					if (decimal < 0) { result.push_back('-'); decimal *= -1; }
					it = buff;

					do { *it++ = '0' + (decimal % 10); decimal/=10; } while (decimal != 0);
					do { result.push_back(*(--it)); } while (it != buff);
					break;

			}

		}
		else {
			result.push_back('%');
			result.push_back(c);
		}

		phase = 0;
	}

	return result;
}
