#include "Version.h"

#if __has_include("AutoVersion.h")
#include "AutoVersion.h"
#else
#define CANDELA_DIRTY
#endif

#ifndef CANDELA_COMMIT
#define CANDELA_COMMIT N/A
#endif

#ifndef CANDELA_DATE
#define CANDELA_DATE N/A
#endif

#ifdef CANDELA_DIRTY
#undef CANDELA_DIRTY
#define CANDELA_DIRTY true
#else
#define CANDELA_DIRTY false
#endif

#define STRINGIFY2(X) #X
#define STRINGIFY(X) STRINGIFY2(X)

const char* candela::version::Commit = STRINGIFY(CANDELA_COMMIT);
const char* candela::version::Date = STRINGIFY(CANDELA_DATE);
const bool candela::version::Dirty = CANDELA_DIRTY;

static char commitSummary[16] = {};

const char* candela::version::CommitSummary()
{
	if (commitSummary[0] == '\0')
	{
		for (int i = 0; i < 7; ++i)
			commitSummary[i] = Commit[i];
		if (Dirty)
		{
			const auto dirty = "-dirty";
			for (int i = 0; i < 6; ++i)
				commitSummary[i + 7] = dirty[i];
		}
	}
	return commitSummary;
}