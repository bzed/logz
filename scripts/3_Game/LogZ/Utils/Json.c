/*
    SPDX-License-Identifier: GPL-3.0-or-later
    Copyright (c) 2025 WoozyMasta
    Copyright (c) 2026 Bernd Zeimetz <bernd@bzed.de>
    Source: https://github.com/woozymasta/logz
*/

#ifdef SERVER
/**
    \brief Misc helpers for JSON handling.
*/
class LogZ_Json
{
	/**
	    \brief Escape a string for safe JSON inclusion.
	    \return string Escaped string (no surrounding quotes).
	*/
	static string Escape(string input)
	{
		if (!input || input == string.Empty)
			return "";

		if (input.IndexOf("\"") == -1 && input.IndexOf("\\") == -1 && input.IndexOf("\n") == -1 && input.IndexOf("\r") == -1 && input.IndexOf("\t") == -1)
			return input;

		string result = "";
		int n = input.LengthUtf8();

		for (int i = 0; i < n; ++i) {
			string chars = input.SubstringUtf8(i, 1);
			int ascii = chars.ToAscii();

			// ASCII specials
			if (ascii == 34) { result += "\\"; result += "\""; continue; }
			if (ascii == 92) { result += "\\"; result += "\\"; continue; }
			if (ascii == 10) { result += "\\n"; continue; }
			if (ascii == 13) { result += "\\r"; continue; }
			if (ascii == 9) { result += "\\t"; continue; }
			if (ascii >= 0 && ascii < 32) { result += " "; continue; }

			result += chars; // keep UTF-8 as-is
		}

		return result;
	}

	/**
	    \brief Reduce an attacker-supplied string to a plain identifier.
	    \details
	        Hit claims carry strings the client chose (surface, ammo type). Escape() lets control
	        characters other than \n \r \t through when the string has no other special, and Log()
	        writes a value that looks like a JSON object, array or number without quotes, so a forged
	        string could produce an invalid line or forge structure. Token() keeps only
	        [A-Za-z0-9_.-], replaces everything else with '?', cuts at maxLen, and prefixes '_' when
	        the result would still read as a number (so it stays a quoted string).
	    \param input  Untrusted string.
	    \param maxLen Maximum length of the result.
	    \return string Safe identifier, empty for empty input.
	*/
	static string Token(string input, int maxLen = 64)
	{
		if (!input || input == string.Empty)
			return "";

		string result = "";
		int n = input.Length();
		if (n > maxLen)
			n = maxLen;

		for (int i = 0; i < n; ++i) {
			int c = input.Get(i).ToAscii();

			// 0-9, A-Z, a-z, '_', '.', '-'
			if ((c >= 48 && c <= 57) || (c >= 65 && c <= 90) || (c >= 97 && c <= 122) || c == 95 || c == 46 || c == 45)
				result += input.Get(i);
			else
				result += "?";
		}

		string probe = result;
		if (IsValue(probe))
			result = "_" + result;

		return result;
	}

	/**
	    \brief Heuristic check if string is a JSON value.
	    \warning Stock JsonSerializer has no polymorphism or field injection.
	        We must guess "does this look like JSON" to inject raw values without double escaping.
	        Hack for speed and correctness. Otherwise it turns into a JSON string clusterfuck.
	        Greetings to the game authors.
	    \return bool
	*/
	static bool IsValue(inout string input)
	{
		if (!input || input == string.Empty)
			return false;

		// trim
		input.TrimInPlace();
		int length = input.Length();
		if (length == 0)
			return false;

		// ? ASCII codes https://www.ascii-code.com/
		int first = input.Get(0).ToAscii();
		int last = input.Get(length - 1).ToAscii();

		// objects/arrays pass-through ([...]/{...})
		if ((first == 91 && last == 93) || (first == 123 && last == 125))
			return true;

		// vector "<1.01, 0, -0.6>" -> "[1.01,0,-0.6]"
		if (first == 60 && last == 62) {
			string vec = input.Substring(1, length - 2);
			vec.Replace(",", " ");
			vec.TrimInPlace();

			// split, compact
			array<string> tok = new array<string>();
			array<string> raw = new array<string>();
			vec.Split(" ", raw);

			for (int i = 0; i < raw.Count(); ++i) {
				string t = raw[i];
				if (!t)
					continue;

				t.TrimInPlace();
				if (t != string.Empty)
					tok.Insert(t);
			}

			// fast numeric check for 3 floats
			if (tok.Count() == 3 && IsNumber(tok[0]) && IsNumber(tok[1]) && IsNumber(tok[2])) {
				input = string.Format("[%1,%2,%3]", tok[0], tok[1], tok[2]);
				return true;
			}

			// not a vector -> treat as plain string
			return false;
		}

		// number (int/float with optional exponent)
		return IsNumber(input);
	}

	/**
	    \brief Check if string is a numeric literal.
	    \return bool True if valid numeric literal.
	*/
	private static bool IsNumber(string input)
	{
		if (!input)
			return false;

		int length = input.Length();
		if (length == 0)
			return false;

		int index = 0;

		// '-'
		if (input.Get(0).ToAscii() == 45) {
			index = 1;
			if (index >= length)
				return false;

			int c1 = input.Get(index).ToAscii();
			if (c1 < 48 || c1 > 57)
				return false;
		}

		bool seenDigit, seenDot, needDigitAfterDot, seenExp;

		for (; index < length; ++index) {
			int c = input.Get(index).ToAscii();

			// digit
			if (c >= 48 && c <= 57) {
				seenDigit = true;
				if (needDigitAfterDot)
					needDigitAfterDot = false;

				continue;
			}

			// '.'
			if (c == 46) {
				if (seenDot || seenExp)
					return false;

				seenDot = true;
				needDigitAfterDot = true; // forbid ending with '.'

				continue;
			}

			// 'e' or 'E'
			if (c == 69 || c == 101) {
				if (seenExp || !seenDigit)
					return false;

				seenExp = true;
				seenDigit = false; // must see digits after exponent

				// optional sign after exponent
				if (index + 1 < length) {
					int cn = input.Get(index + 1).ToAscii();
					if (cn == 43 || cn == 45) // '+' or '-'
						index++;
				}

				continue;
			}

			// any other char => not a number
			return false;
		}

		if (needDigitAfterDot)
			return false;

		return seenDigit;
	}
}
#endif
