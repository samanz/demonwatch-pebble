/* Resource-backed Doom64KB data. Permanent lumps retain stable addresses;
 * graphics are valid until the next graphics request. No flash pointers escape. */
#include <pebble.h>
#include <stdint.h>
#include <string.h>
#undef false
#undef true
#include "../doom/doomtype.h"
#include "../doom/w_wad.h"
#include "../doom/z_zone.h"
#include "../doom/i_system.h"

extern void *malloc(size_t); extern void *calloc(size_t, size_t);
typedef struct { int32_t offset, size; char name[8]; } lump_t;
static ResHandle resource;
static lump_t *directory;
static void **resident;
static int32_t count;
static uint32_t wad_size, scratch_size;
static uint8_t *scratch;
static int16_t scratch_lump = -1, pstart, pend, sstart, send;

static void read_exact(uint32_t offset, void *dest, uint32_t size) {
    if (offset > wad_size || size > wad_size - offset ||
        resource_load_byte_range(resource, offset, dest, size) != size)
        I_Error("Resource read %lu+%lu", offset, size);
}
static lump_t *checked(int16_t num) {
    if (num < 0 || num >= count) {
        app_log(1,"wad",0,"Invalid lump %d",num);
        I_Error("Invalid lump");
    }
    return &directory[num];
}
int16_t W_GetNumForName(const char *name) {
    char key[8] = {0};
    for (int i=0; i<8 && name[i]; ++i)
        key[i] = name[i] >= 'a' && name[i] <= 'z' ? name[i]-32 : name[i];
    for (int i=count-1; i>=0; --i)
        if (!memcmp(directory[i].name, key, 8)) return i;
    return -1;
}
static int graphics_lump(int n) {
    return (n > pstart && n < pend) || (n > sstart && n < send);
}
void W_Init(void) {
    resource = resource_get_handle(RESOURCE_ID_E1M1_WAD);
    wad_size = resource_size(resource);
    struct { char magic[4]; int32_t count, offset; } header;
    read_exact(0, &header, sizeof(header));
    if ((memcmp(header.magic,"IWAD",4) && memcmp(header.magic,"PWAD",4)) ||
        header.count <= 0 || header.count > 32767 || header.offset < 0)
        I_Error("Invalid WAD header");
    count = header.count;
    directory = malloc(count * sizeof(*directory));
    resident = calloc(count, sizeof(*resident));
    if (!directory || !resident) I_Error("WAD directory allocation");
    read_exact(header.offset, directory, count * sizeof(*directory));
    pstart = W_GetNumForName("P_START"); pend = W_GetNumForName("P_END");
    sstart = W_GetNumForName("S_START"); send = W_GetNumForName("S_END");
    if (pstart < 0 || pend <= pstart || sstart < 0 || send <= sstart)
        I_Error("Missing graphics markers");
    for (int i=0; i<count; ++i) {
        lump_t *l = &directory[i];
        if (l->offset < 0 || l->size < 0 || l->size > 65535 ||
            (uint32_t)l->offset > wad_size || (uint32_t)l->size > wad_size-l->offset)
            I_Error("Invalid WAD directory entry %d", i);
        if (graphics_lump(i) && (uint32_t)l->size > scratch_size) scratch_size=l->size;
    }
    scratch = malloc(scratch_size ? scratch_size : 1);
    if (!scratch) I_Error("Graphics scratch allocation %lu", scratch_size);
    app_log(100,"wad",0,"WAD: %ld lumps, scratch %lu",count,scratch_size);
}
const char *W_GetNameForNum(int16_t num) { return checked(num)->name; }
uint16_t W_LumpLength(int16_t num) { return checked(num)->size; }
void W_ReadLumpByNum(int16_t num, void *dest) {
    lump_t *l=checked(num); read_exact(l->offset,dest,l->size);
}
const void *W_GetLumpByNum(int16_t num) {
    lump_t *l=checked(num);
    if (graphics_lump(num)) {
        if (scratch_lump != num) {
            read_exact(l->offset,scratch,l->size);
            scratch_lump=num;
        }
        return scratch;
    }
    if (!resident[num]) {
        resident[num]=Z_MallocStatic(l->size ? l->size : 1);
        read_exact(l->offset,resident[num],l->size);
    }
    return resident[num];
}
