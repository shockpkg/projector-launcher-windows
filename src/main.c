#include <windows.h>

const DWORD errorIcon = MB_ICONERROR;
const LPWSTR errorTitle = L"Error";
const LPWSTR errorGeneric = L"Application not found.";
const LPWSTR errorPath = L"Application not found:\r\n";

inline size_t wstrSize(size_t count) {
	return sizeof(WCHAR) * count;
}

inline LPWSTR wstrAlloc(size_t len) {
	return (LPWSTR)malloc(wstrSize(len + 1));
}

LPWSTR wstrCopy(LPWSTR dest, const LPWSTR source, size_t len) {
	size_t size = wstrSize(len);
	memcpy(dest, source, size);
	dest[len] = L'\0';
	return &(dest[len]);
}

int wstrHasChars(const LPWSTR str, const LPWSTR chars) {
	for (LPWSTR p = chars; *p; p++) {
		if (wcsrchr(str, *p)) {
			return 1;
		}
	}
	return 0;
}

size_t argQuoteRequired(LPWSTR arg) {
	return !*arg || wstrHasChars(arg, L" \"\t\n\v");
}

size_t argQuoteLen(LPWSTR arg) {
	size_t r = 2;
	for (LPWSTR p = arg;; p++) {
		size_t bs = 0;
		for (; *p == L'\\'; p++) {
			bs++;
		}
		if (!*p) {
			r += bs * 2;
			break;
		}
		else if (*p == L'"') {
			r += (bs * 2) + 2;
		}
		else {
			r += bs + 1;
		}
	}
	return r;
}

LPWSTR argQuoteInto(LPWSTR quoted, LPWSTR arg) {
	LPWSTR c = quoted;
	*(c++) = L'"';
	for (LPWSTR p = arg;; p++) {
		size_t bs = 0;
		for (; *p == L'\\'; p++) {
			bs++;
		}
		if (!*p) {
			for (bs *= 2; bs--;) {
				*(c++) = L'\\';
			}
			break;
		}
		else if (*p == L'"') {
			for (bs = ((bs * 2) + 1); bs--;) {
				*(c++) = L'\\';
			}
			*(c++) = *p;
		}
		else {
			while (bs--) {
				*(c++) = L'\\';
			}
			*(c++) = *p;
		}
	}
	*(c++) = L'"';
	*c = L'\0';
	return c;
}

LPWSTR argQuote(LPWSTR arg) {
	LPWSTR quoted = wstrAlloc(argQuoteLen(arg));
	if (quoted) {
		argQuoteInto(quoted, arg);
	}
	return quoted;
}

size_t argEscapeLen(LPWSTR arg, int force) {
	if (!force && !argQuoteRequired(arg)) {
		return wcslen(arg);
	}
	return argQuoteLen(arg);
}

LPWSTR argEscapeInto(LPWSTR escaped, LPWSTR arg, int force) {
	if (!force && !argQuoteRequired(arg)) {
		return wstrCopy(escaped, arg, wcslen(arg));
	}
	return argQuoteInto(escaped, arg);
}

LPWSTR argEscape(LPWSTR arg, int force) {
	LPWSTR escaped = wstrAlloc(argEscapeLen(arg, force));
	if (escaped) {
		argEscapeInto(escaped, arg, force);
	}
	return escaped;
}

LPWSTR argvToCommandLine(int argc, const LPWSTR * argv, int force) {
	size_t len = 0;
	for (int i = 0; i < argc; i++) {
		len += argEscapeLen(argv[i], force) + (i ? 1 : 0);
	}
	LPWSTR cl = wstrAlloc(len);
	if (!cl) {
		return NULL;
	}
	LPWSTR p = cl;
	for (int i = 0; i < argc; i++) {
		if (i) {
			*(p++) = L' ';
		}
		p = argEscapeInto(p, argv[i], force);
	}
	return cl;
}

int startProcess(const LPWSTR path, int argc, const LPWSTR * argv) {
	LPWSTR commandLine = argvToCommandLine(argc, argv, 0);
	if (!commandLine) {
		return 0;
	}

	STARTUPINFOW si;
	ZeroMemory(&si, sizeof(si));
	si.cb = sizeof(si);

	PROCESS_INFORMATION pp;
	ZeroMemory(&pp, sizeof(pp));

	int r = !!CreateProcessW(
		path, commandLine, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pp
	);
	free(commandLine);
	return r;
}

void errorNotFound(const LPWSTR path) {
	// If a path passed, include in the message if possible.
	LPWSTR msg = NULL;
	size_t pathL = path ? wcslen(path) : 0;
	if (pathL) {
		size_t errorPathL = wcslen(errorPath);
		msg = wstrAlloc(errorPathL + pathL);
		if (msg) {
			wstrCopy(wstrCopy(msg, errorPath, errorPathL), path, pathL);
		}
	}

	MessageBoxW(NULL, msg ? msg : errorGeneric, errorTitle, errorIcon);

	if (msg) {
		free(msg);
	}
}

LPWSTR getSelfPath(size_t * length) {
	if (length) {
		*length = 0;
	}

	// Get path to this binary, increasing the size if necessary.
	for (size_t size = MAX_PATH;; size *= 2) {
		// Allocate memory for path or fail.
		LPWSTR data = wstrAlloc(size);
		if (!data) {
			return NULL;
		}

		// Returns greater than 0 and less than limit on success.
		DWORD l = GetModuleFileNameW(NULL, data, size);
		if (l > 0 && l < size) {
			if (length) {
				*length = l;
			}
			return data;
		}
		free(data);
	}
}

const LPWSTR getPathFile(LPWSTR path, size_t * length) {
	if (length) {
		*length = 0;
	}

	size_t offset = 0;
	for (size_t i = 0;; i++) {
		// At the null character return path since offset.
		if (!path[i]) {
			if (length) {
				*length = i - offset;
			}
			return &(path[offset]);
		}

		// Start after last slash.
		if (path[i] == L'\\') {
			offset = i + 1;
		}
	}
}

LPWSTR getApplicationPath() {
	// Get path to self or fail.
	size_t selfL;
	LPWSTR self = getSelfPath(&selfL);
	if (!self) {
		return NULL;
	}

	// Check the length and file extension.
	size_t selfLNoExt = selfL - 4;
	if (!(
		selfL > 4 &&
		self[selfLNoExt - 1] != L'\\' &&
		self[selfLNoExt] == L'.' &&
		(self[selfLNoExt + 1] == L'e' || self[selfLNoExt + 1] == L'E') &&
		(self[selfLNoExt + 2] == L'x' || self[selfLNoExt + 2] == L'X') &&
		(self[selfLNoExt + 3] == L'e' || self[selfLNoExt + 3] == L'E')
	)) {
		free(self);
		return NULL;
	}

	// Find the file name and length of it.
	size_t nameL;
	const LPWSTR name = getPathFile(self, &nameL);

	// Create memory for the full path or fail.
	size_t pathL = selfLNoExt + 1 + nameL;
	LPWSTR path = wstrAlloc(pathL);
	if (!path) {
		free(self);
		return NULL;
	}

	// Assemble path.
	LPWSTR p = wstrCopy(path, self, selfLNoExt);
	*(p++) = L'\\';
	wstrCopy(p, name, nameL);

	free(self);
	return path;
}

LPWSTR * argvCopy(int argc, const LPWSTR * argv) {
	size_t size = sizeof(LPWSTR *) * (argc + 1);
	LPWSTR * args = (LPWSTR *)malloc(size);
	return args ? memcpy(args, argv, size) : args;
}

int entry(int argc, const LPWSTR * argv) {
	LPWSTR path = NULL;
	int r = 1;

	do {
		// Check that the arguments were parsed.
		if (!argc) {
			break;
		}

		// Resolve the path to the application to run.
		path = getApplicationPath();
		if (!path) {
			break;
		}

		// Copy argument pointers and replace first or fail.
		LPWSTR * args = argvCopy(argc, argv);
		if (!args) {
			break;
		}
		args[0] = path;

		// Start process if possible.
		if (startProcess(path, argc, args)) {
			r = 0;
		}
		free(args);
	}
	while (0);

	// If error return code, show error with path if available.
	if (r) {
		errorNotFound(path);
	}
	if (path) {
		free(path);
	}
	return r;
}

int WINAPI WinMain(
	HINSTANCE hInstance,
	HINSTANCE hPrevInstance,
	LPSTR lpCmdLine,
	int nShowCmd
) {
	int argvwc = 0;
	LPWSTR * argvw = CommandLineToArgvW(GetCommandLineW(), &argvwc);
	int r = entry(argvw ? argvwc : 0, argvw);
	if (argvw) {
		LocalFree(argvw);
	}
	return r;
}
