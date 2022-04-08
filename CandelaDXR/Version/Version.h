#pragma once

namespace candela::version 
{
    extern const char* Commit;
    extern const char* Date;
    extern const bool Dirty;

    const char* CommitSummary();
}
