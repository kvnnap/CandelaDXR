#include "Version.h"

#if __has_include("AutoVersion.h")
#include "AutoVersion.h"
#else
#define CANDELA_COMMIT N/A
#define CANDELA_DATE N/A
#define CANDELA_DIRTY
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
		const auto dirty = "-dirty";
		for (int i = 0; i < 6; ++i)
			commitSummary[i + 7] = dirty[i];
	}
	return commitSummary;
}