/* Command-line tokens are ASCII. Avoid newlib locale/ctype tables: the
 * standalone archive implementation embeds addresses outside Pebble's API. */
int strcasecmp(const char *a, const char *b) {
    unsigned char x, y;
    do {
        x = (unsigned char)*a++;
        y = (unsigned char)*b++;
        if (x >= 'A' && x <= 'Z') x += 'a' - 'A';
        if (y >= 'A' && y <= 'Z') y += 'a' - 'A';
        if (x != y) return (int)x - (int)y;
    } while (x);
    return 0;
}
