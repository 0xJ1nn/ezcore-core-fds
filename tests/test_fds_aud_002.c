#include "ezhle/ezhle.h"
#include "ezhle/ezhle_fds.h"

#include <stdio.h>
#include <string.h>

static int name_has_audio(const char *name)
{
    if (name == NULL) {
        return 0;
    }
    /* case-insensitive substring checks for audio-related service names */
    const char *p = name;
    /* lower-case scan */
    char buf[64];
    size_t i = 0;
    while (p[i] != '\0' && i + 1 < sizeof(buf)) {
        char c = p[i];
        if (c >= 'A' && c <= 'Z') {
            c = (char)(c + ('a' - 'A'));
        }
        buf[i] = c;
        i++;
    }
    buf[i] = '\0';
    return (strstr(buf, "audio") != NULL) ||
           (strstr(buf, "sound") != NULL) ||
           (strstr(buf, "wavetable") != NULL) ||
           (strstr(buf, "apu") != NULL);
}

int main(void)
{
    size_t n = ezhle_fds_entry_count();
    const ezhle_fds_entry_t *t = ezhle_fds_entry_table();
    size_t i;

    if (n == 0 || t == NULL) {
        printf("FDS-AUD-002: empty entry table\n");
        return 1;
    }
    for (i = 0; i < n; i++) {
        if (name_has_audio(t[i].name)) {
            printf("FDS-AUD-002: unexpected audio entry %s\n", t[i].name);
            return 1;
        }
    }

    /* The public ABI has no audio/wavetable channel hook: ezhle_bus_t and
     * ezhle_desc_t expose only memory + log/trace + scheduler surfaces. */
    printf("FDS-AUD-002: no FDS audio hook in ABI (blocked) PASS\n");
    return 0;
}
