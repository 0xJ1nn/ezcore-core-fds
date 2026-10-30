// C++ consumer smoke test.
//
// The core is C11, but every public header must be usable from C++ through
// extern "C". This test fails to link if a guard is missing.
#include <cstdio>

#include <ezhle/ezhle.h>
#include <ezhle/ezhle_fds.h>
#include <ezhle/ezhle_ps2.h>

int main()
{
    const size_t count = ezhle_fds_entry_count();
    const ezhle_fds_entry_t *table = ezhle_fds_entry_table();

    if (table == nullptr || count != 36u) {
        std::fprintf(stderr, "cpp smoke: bad FDS entry table (%zu)\n", count);
        return 1;
    }
    std::printf("cpp smoke: C++17 link OK, %zu FDS entries\n", count);
    return 0;
}
