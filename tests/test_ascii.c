#include <assert.h>
int strcasecmp(const char *, const char *);
int main(void) {
    assert(strcasecmp("-WARP", "-warp") == 0);
    assert(strcasecmp("", "") == 0);
    assert(strcasecmp("-war", "-warp") < 0);
    assert(strcasecmp("-warp", "-war") > 0);
    assert(strcasecmp("a", "B") < 0);
    assert(strcasecmp("\200", "\177") > 0);
    return 0;
}
