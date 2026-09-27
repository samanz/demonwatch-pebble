/* Checks the packed states[] and mobjinfo[] tables: every index they hold
 * must be in range. Catches initializer/field-order mismatches, which the
 * watch build (-w) would compile silently.
 * Build: gcc -std=c11 -fshort-enums -DPEBBLE_EMERY -Isrc/doom -Itests
 *        tests/test_info.c -Wl,--unresolved-symbols=ignore-all */
#include <assert.h>
#include <stdio.h>
#include "../src/doom/info.c"   /* for the file-local NUMACTIONFUNCS */
#undef printf                   /* the engine's stdio shim silences it */
int printf(const char *, ...);

int main(void) {
    const int actions = NUMACTIONFUNCS;
    assert(sizeof(state_t) == 8);
    for (int i = 1; i < actions; ++i) assert(actionfuncs[i]);
    for (int i = 0; i < NUMSTATES; ++i) {
        const state_t *s = &states[i];
        assert(s->sprite >= 0 && s->sprite < NUMSPRITES);
        assert((s->frame & 0x7fff) < 29);
        assert(s->action < actions);
        assert(s->nextstate < NUMSTATES);
        assert(s->tics >= -1);
    }
    for (int i = 0; i < NUMMOBJTYPES; ++i) {
        const mobjinfo_t *m = &mobjinfo[i];
        assert(m->spawnstate < NUMSTATES && m->seestate < NUMSTATES);
        assert(m->painstate < NUMSTATES && m->deathstate < NUMSTATES);
        assert(m->meleestate < NUMSTATES && m->missilestate < NUMSTATES);
    }
    for (int i = 0; i < NUMSPRITES; ++i) assert(sprnames[i][4] == 0 && sprnames[i][3]);
    printf("PASS: %d states, %d actions, %d thing types, %d sprites in range\n",
           NUMSTATES, actions - 1, NUMMOBJTYPES, NUMSPRITES);
    return 0;
}
