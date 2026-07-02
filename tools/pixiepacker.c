// PixiePacker - converts a PNG into packed byte data + a C header for
// Fuselage's GDMF Pixie subsystem (PIXIE_OP_UNPACK). Build-time-only tool;
// see pixiepackertool.txt in this folder for the full design writeup and
// GDMF/pixie_plans.txt in the main project for the engine-side design.
//
// This tool lives outside any one dated project snapshot (tools/ is a
// sibling to the dated Fuselage/<date>/ folders, not nested inside one),
// so it keeps its own local copy of PixieUnpackFormat/PixiePackPass rather
// than #include-ing GDMF/gdmf_pixies.h across a snapshot boundary. The
// numeric values below MUST stay in sync with gdmf_pixies.h's enums of
// the same names.
//
// Uses stb_image.h (public domain / MIT) to decode the source PNG, 
// and stb_image_write.h for the optional --preview round-trip 
// verification. Both are build-time-only dependencies of this
// tool. Fuselage itself never depends on either.

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <ctype.h>

typedef struct { unsigned char r, g, b, a; } Color;

// Must match GDMF/gdmf_pixies.h's PixieUnpackFormat.
typedef enum {
    PIXIE_FORMAT_RGBA8              = 0,
    PIXIE_FORMAT_PALETTE4BPP        = 1,
    PIXIE_FORMAT_RLE_PALETTE_SHARED = 2,
    PIXIE_FORMAT_RLE_PALETTE_OWN    = 3,
    PIXIE_FORMAT_RLE_RGBA8          = 4,
} PixieUnpackFormat;

// Must match GDMF/gdmf_pixies.h's PixiePackPass. The enum exists so more
// passes can be appended later without touching already-packed assets
// (see pixiepackertool.txt). PIXIE_PASS_HUFFMAN, when used, always wraps
// PIXIE_PASS_RLE's output -- it entropy-codes whatever byte stream came
// before it, generically, with no knowledge of what that stream means.
// PIXIE_PASS_DELTA is the opposite end of the stack -- always the FIRST
// pass applied (nothing wraps it, it wraps nothing), only ever used for
// PIXIE_FORMAT_RLE_RGBA8's per-channel planes.
// PIXIE_PASS_LZ is an ALTERNATIVE to PIXIE_PASS_RLE, never stacked with
// it -- the packer tries both and keeps whichever compresses smaller.
#define PIXIE_PASS_RLE     0
#define PIXIE_PASS_HUFFMAN 1
#define PIXIE_PASS_DELTA   2
#define PIXIE_PASS_LZ      3

#define MAX_UNIQUE_COLORS 256
#define MAX_IMAGES_PER_RUN 64

// ---------------------------------------------------------------------
// Small growable byte buffer -- used to assemble each image's packed
// blob piece by piece without pre-computing every size by hand.
// ---------------------------------------------------------------------
typedef struct {
    unsigned char* data;
    size_t         len;
    size_t         cap;
} ByteBuf;

static void buf_init(ByteBuf* b) {
    b->data = NULL;
    b->len  = 0;
    b->cap  = 0;
}

static void buf_push(ByteBuf* b, const void* src, size_t n) {
    if (b->len + n > b->cap) {
        size_t newCap = b->cap ? b->cap * 2 : 256;
        while (newCap < b->len + n) { newCap *= 2; }
        b->data = realloc(b->data, newCap);
        b->cap  = newCap;
    }
    memcpy(b->data + b->len, src, n);
    b->len += n;
}

static void buf_push_u8(ByteBuf* b, uint8_t v)   { buf_push(b, &v, 1); }
static void buf_push_u16(ByteBuf* b, uint16_t v) { buf_push(b, &v, 2); }
static void buf_push_u32(ByteBuf* b, uint32_t v) { buf_push(b, &v, 4); }

// ---------------------------------------------------------------------
// Delta / URR ("Unrepeated Runs") -- pass 2, always the FIRST pass
// applied (nothing comes before it). Predicts each byte from the one
// immediately before it in scan order (no row-width awareness -- the
// codec already treats a channel plane as one flat array, and resetting
// prediction at each row start would be new complexity for a one-byte-
// per-row saving) and stores the signed residual, wrapped into a byte.
// Fully lossless and harmless on data that isn't gradient-like: worst
// case the residuals are just as random as the source bytes were.
//
// Only ever used for PIXIE_FORMAT_RLE_RGBA8's per-channel planes -- a
// true-color gradient defeats RLE almost entirely (adjacent bytes rarely
// identical), but the same gradient turns into small near-zero residuals
// under delta, which RLE/Huffman then compress well. NOT used for the
// two palette-indexed tiers: palette indices are categorical (whatever
// order peek_unique_colors happened to encounter colors in), not
// numerically continuous, so "diffing" them is meaningless.
// ---------------------------------------------------------------------
static void delta_encode(const unsigned char* data, size_t count, unsigned char* out) {
    unsigned char prev = 0;
    for (size_t i = 0; i < count; i++) {
        out[i] = (unsigned char)(data[i] - prev);
        prev = data[i];
    }
}

static void delta_decode(const unsigned char* data, size_t count, unsigned char* out) {
    unsigned char prev = 0;
    for (size_t i = 0; i < count; i++) {
        prev = (unsigned char)(prev + data[i]);
        out[i] = prev;
    }
}

// ---------------------------------------------------------------------
// RLE (pass 0). Byte-oriented (count, value) pairs, count in [1,255].
// Decode is a single pass, no intermediate buffer -- exactly what the
// engine-side decoder needs to do too, see pixiepackertool.txt.
// ---------------------------------------------------------------------
static void rle_encode(const unsigned char* data, size_t count, ByteBuf* out) {
    size_t i = 0;
    while (i < count) {
        unsigned char v = data[i];
        size_t run = 1;
        while (i + run < count && data[i + run] == v && run < 255) { run++; }
        buf_push_u8(out, (uint8_t)run);
        buf_push_u8(out, v);
        i += run;
    }
}

// Used only for the auto-mode "did RLE actually help" check and for
// --preview round-tripping -- not needed by the engine, which only ever
// decodes forward.
static void rle_decode(const unsigned char* data, size_t len, unsigned char* out) {
    size_t i = 0, o = 0;
    while (i < len) {
        unsigned char run = data[i++];
        unsigned char v   = data[i++];
        memset(out + o, v, run);
        o += run;
    }
}

// ---------------------------------------------------------------------
// LZ (pass 3) -- an ALTERNATIVE to RLE, never stacked with it: a run of
// one repeated byte is just a match with offset 1, so LZ strictly
// subsumes what RLE catches, plus non-adjacent repeated patterns RLE can
// never see. The packer tries both per asset and keeps whichever
// compresses smaller (see pack_image).
//
// Byte-oriented token stream, no bit-packing (consistent with RLE --
// Huffman is still the only pass that does bit-level work):
//   tag byte, high bit 0: literal run. Low 7 bits = count (1-127),
//     `count` raw bytes follow.
//   tag byte, high bit 1: match. Low 7 bits = length-LZ_MIN_MATCH
//     (0-127, so length 3-130), a 4-byte back-offset follows (distance
//     from the current output position to the start of the earlier
//     copy -- always 1..o, never 0, never past the start of the
//     stream). A single match longer than 130 bytes is simply split
//     across multiple consecutive match tokens.
// ---------------------------------------------------------------------
#define LZ_MIN_MATCH   3
#define LZ_MAX_MATCH   130   // 127 (max low-7-bit value) + LZ_MIN_MATCH
#define LZ_MAX_LITERAL 127
#define LZ_HASH_BITS   16
#define LZ_HASH_SIZE   (1 << LZ_HASH_BITS)
#define LZ_MAX_CHAIN   32    // candidates tried per position -- greedy, not an optimal parse

static uint32_t lz_hash3(const unsigned char* p) {
    uint32_t v = ((uint32_t)p[0] << 16) | ((uint32_t)p[1] << 8) | (uint32_t)p[2];
    return (v * 2654435761u) >> (32 - LZ_HASH_BITS);
}

static void lz_flush_literals(ByteBuf* out, const unsigned char* data, size_t start, size_t end) {
    while (start < end) {
        size_t chunk = end - start;
        if (chunk > LZ_MAX_LITERAL) { chunk = LZ_MAX_LITERAL; }
        buf_push_u8(out, (uint8_t)chunk);
        buf_push(out, data + start, chunk);
        start += chunk;
    }
}

// Greedy hash-chain match finder -- not an optimal parser (that's real
// complexity, like zopfli's, for marginal extra gain). At each position,
// hashes the next 3 bytes, walks up to LZ_MAX_CHAIN previous positions
// sharing that hash, and takes the longest match found among them.
static void lz_encode(const unsigned char* data, size_t count, ByteBuf* out) {
    int* head = malloc(sizeof(int) * LZ_HASH_SIZE);
    int* prev = malloc(sizeof(int) * (count > 0 ? count : 1));
    for (int h = 0; h < LZ_HASH_SIZE; h++) { head[h] = -1; }

    size_t i = 0;
    size_t literalStart = 0;

    while (i < count) {
        size_t bestLen = 0;
        size_t bestOffset = 0;

        if (i + LZ_MIN_MATCH <= count) {
            uint32_t h = lz_hash3(data + i);
            int cand = head[h];
            int chainSteps = 0;
            size_t maxLen = count - i;
            if (maxLen > LZ_MAX_MATCH) { maxLen = LZ_MAX_MATCH; }

            while (cand >= 0 && chainSteps < LZ_MAX_CHAIN) {
                size_t candPos = (size_t)cand;
                size_t len = 0;
                while (len < maxLen && data[candPos + len] == data[i + len]) { len++; }
                if (len > bestLen) {
                    bestLen = len;
                    bestOffset = i - candPos;
                }
                cand = prev[candPos];
                chainSteps++;
            }
        }

        if (bestLen >= LZ_MIN_MATCH) {
            lz_flush_literals(out, data, literalStart, i);

            buf_push_u8(out, (uint8_t)(0x80 | (bestLen - LZ_MIN_MATCH)));
            buf_push_u32(out, (uint32_t)bestOffset);

            // Index every position the match covers (not just its start)
            // so a later match can reference into the middle of this one.
            size_t matchEnd = i + bestLen;
            while (i < matchEnd) {
                if (i + LZ_MIN_MATCH <= count) {
                    uint32_t h = lz_hash3(data + i);
                    prev[i] = head[h];
                    head[h] = (int)i;
                }
                i++;
            }
            literalStart = i;
        } else {
            if (i + LZ_MIN_MATCH <= count) {
                uint32_t h = lz_hash3(data + i);
                prev[i] = head[h];
                head[h] = (int)i;
            }
            i++;
        }
    }
    lz_flush_literals(out, data, literalStart, count);

    free(head);
    free(prev);
}

// Used only for the auto-mode "did LZ actually help" check and for
// --preview round-tripping -- not needed by the engine, which only ever
// decodes forward. Match copies are done byte-by-byte (not memcpy) since
// offset < length is common and expected (that's how a repeated run gets
// represented at all) -- an overlapping memcpy would be undefined
// behavior, a byte-by-byte loop is well-defined and exactly what a
// self-referential copy needs.
static uint32_t lz_read_u32(const unsigned char* p) {
    uint32_t v;
    memcpy(&v, p, sizeof(v));
    return v;
}

static void lz_decode(const unsigned char* data, size_t len, unsigned char* out) {
    size_t i = 0, o = 0;
    while (i < len) {
        uint8_t tag = data[i++];
        if (tag & 0x80) {
            size_t matchLen = (size_t)(tag & 0x7F) + LZ_MIN_MATCH;
            uint32_t offset = lz_read_u32(data + i);
            i += 4;
            for (size_t k = 0; k < matchLen; k++) {
                out[o] = out[o - offset];
                o++;
            }
        } else {
            size_t litLen = tag;
            memcpy(out + o, data + i, litLen);
            i += litLen;
            o += litLen;
        }
    }
}

// ---------------------------------------------------------------------
// Huffman (pass 1). Canonical Huffman over whatever byte stream the
// previous pass produced -- generic, no knowledge of what the bytes mean
// (RLE tokens, in current usage). Canonical means we only ever store 256
// code LENGTHS (one per possible byte value, 0 = unused), not the codes
// or the tree itself -- the decoder rebuilds the exact same code
// assignment from the length table alone (same trick DEFLATE/JPEG use).
// ---------------------------------------------------------------------

// At most 256 leaves (one per byte value) and, since combining N leaves
// via binary merges always takes exactly N-1 merges, at most 255
// internal nodes -- 511 total, fixed and known ahead of time.
#define HUFFMAN_MAX_NODES 511

typedef struct {
    uint32_t freq;
    int      parent;  // -1 until merged into a parent; unused for internal nodes once set
} HuffmanNode;

// Computes each symbol's Huffman code length via the standard "repeatedly
// merge the two lowest-frequency active nodes" build. Uses a flat O(n^2)
// scan for the two smallest nodes each round rather than a real priority
// queue -- with at most 256 symbols and this being a build-time-only
// tool, that's a non-issue, and it keeps this self-contained.
static void huffman_build_code_lengths(const uint32_t freq[256], uint8_t codeLengths[256]) {
    memset(codeLengths, 0, 256);

    HuffmanNode nodes[HUFFMAN_MAX_NODES];
    int nodeCount = 0;
    int active[256];
    int activeCount = 0;

    for (int s = 0; s < 256; s++) {
        if (freq[s] > 0) {
            nodes[nodeCount].freq = freq[s];
            nodes[nodeCount].parent = -1;
            active[activeCount++] = nodeCount;
            nodeCount++;
        }
    }

    if (activeCount == 0) {
        return;  // nothing to encode
    }
    if (activeCount == 1) {
        // Degenerate single-symbol alphabet -- a real binary tree can't
        // be built from one leaf (no merge ever happens), so force a
        // 1-bit code rather than leaving it at length 0 (which would be
        // ambiguous to decode -- a 0-bit "code" can't be told apart from
        // "no code read yet").
        for (int s = 0; s < 256; s++) {
            if (freq[s] > 0) { codeLengths[s] = 1; break; }
        }
        return;
    }

    while (activeCount > 1) {
        int i1 = 0;
        for (int i = 1; i < activeCount; i++) {
            if (nodes[active[i]].freq < nodes[active[i1]].freq) { i1 = i; }
        }
        int n1 = active[i1];
        active[i1] = active[--activeCount];

        int i2 = 0;
        for (int i = 1; i < activeCount; i++) {
            if (nodes[active[i]].freq < nodes[active[i2]].freq) { i2 = i; }
        }
        int n2 = active[i2];
        active[i2] = active[--activeCount];

        int parent = nodeCount++;
        nodes[parent].freq = nodes[n1].freq + nodes[n2].freq;
        nodes[parent].parent = -1;
        nodes[n1].parent = parent;
        nodes[n2].parent = parent;

        active[activeCount++] = parent;
    }

    // Code length of each leaf = its depth from root, found by walking
    // parent links. Leaves were pushed into nodes[0..activeCountOriginal)
    // in byte-value order above, so leafIndex tracks that same order.
    int leafIndex = 0;
    for (int s = 0; s < 256; s++) {
        if (freq[s] > 0) {
            int depth = 0;
            int n = leafIndex;
            while (nodes[n].parent != -1) {
                n = nodes[n].parent;
                depth++;
            }
            codeLengths[s] = (uint8_t)depth;
            leafIndex++;
        }
    }
}

// Assigns canonical codes from a code-length table: codes ordered by
// (length, symbol value), consecutive within each length -- the exact
// convention RFC 1951 (DEFLATE) section 3.2.2 uses, chosen here for the
// same reason: it's the standard way to make a code table reconstructible
// from lengths alone with no ambiguity.
static void huffman_assign_canonical_codes(const uint8_t codeLengths[256], uint32_t codes[256]) {
    int blCount[256] = { 0 };
    for (int s = 0; s < 256; s++) {
        if (codeLengths[s] > 0) { blCount[codeLengths[s]]++; }
    }

    uint32_t code = 0;
    uint32_t firstCode[256] = { 0 };
    for (int len = 1; len < 256; len++) {
        code = (code + (uint32_t)blCount[len - 1]) << 1;
        firstCode[len] = code;
    }

    uint32_t nextCode[256];
    memcpy(nextCode, firstCode, sizeof(nextCode));
    for (int s = 0; s < 256; s++) {
        if (codeLengths[s] > 0) {
            codes[s] = nextCode[codeLengths[s]]++;
        }
    }
}

// Minimal MSB-first bit writer -- bits of a code are written most-
// significant-bit first (matching the canonical assignment above), and
// bits accumulate into bytes most-significant-bit first too. The final
// partial byte, if any, is zero-padded (bitwriter_flush).
typedef struct {
    ByteBuf* out;
    uint32_t bitBuf;
    int      bitCount;
} BitWriter;

static void bitwriter_init(BitWriter* bw, ByteBuf* out) {
    bw->out = out;
    bw->bitBuf = 0;
    bw->bitCount = 0;
}

static void bitwriter_write(BitWriter* bw, uint32_t value, int len) {
    for (int i = len - 1; i >= 0; i--) {
        bw->bitBuf = (bw->bitBuf << 1) | ((value >> i) & 1u);
        bw->bitCount++;
        if (bw->bitCount == 8) {
            buf_push_u8(bw->out, (uint8_t)bw->bitBuf);
            bw->bitBuf = 0;
            bw->bitCount = 0;
        }
    }
}

static void bitwriter_flush(BitWriter* bw) {
    if (bw->bitCount > 0) {
        buf_push_u8(bw->out, (uint8_t)(bw->bitBuf << (8 - bw->bitCount)));
        bw->bitBuf = 0;
        bw->bitCount = 0;
    }
}

// Huffman-encodes `data` into `out` as a self-contained unit: 256 code-
// length bytes, then decoded length (how many bytes `data` was) and
// encoded length (how many bit-packed bytes follow), then the payload
// itself. Always succeeds -- whether the result is actually smaller than
// `len` is the caller's decision (same fallback-if-it-doesn't-help
// pattern already used for RLE itself), not this function's.
static void huffman_encode(const unsigned char* data, size_t len, ByteBuf* out) {
    uint32_t freq[256] = { 0 };
    for (size_t i = 0; i < len; i++) { freq[data[i]]++; }

    uint8_t codeLengths[256];
    huffman_build_code_lengths(freq, codeLengths);

    uint32_t codes[256] = { 0 };
    huffman_assign_canonical_codes(codeLengths, codes);

    buf_push(out, codeLengths, 256);

    ByteBuf bits;
    buf_init(&bits);
    BitWriter bw;
    bitwriter_init(&bw, &bits);
    for (size_t i = 0; i < len; i++) {
        unsigned char sym = data[i];
        bitwriter_write(&bw, codes[sym], codeLengths[sym]);
    }
    bitwriter_flush(&bw);

    buf_push_u32(out, (uint32_t)len);        // decoded length
    buf_push_u32(out, (uint32_t)bits.len);   // encoded (bit-packed) length
    buf_push(out, bits.data, bits.len);
    free(bits.data);
}

// Huffman decode -- used only by --preview (to verify round-trip
// correctness without running the engine) and the auto-mode "did it
// help" check doesn't need this at all (that's a size comparison, not a
// decode). Mirrors GDMF/gdmf_pixies.c's pixie_huffman_decode exactly;
// this tool operates on its own just-encoded in-memory blob rather than
// untrusted RAM, so it skips that side's bounds-checking, but the
// decode algorithm itself must stay byte-for-byte identical or --preview
// wouldn't actually verify anything.
typedef struct {
    const unsigned char* data;
    size_t len;
    size_t bytePos;
    int    bitPos;
} BitReader;

static int bitreader_read_bit(BitReader* br) {
    if (br->bytePos >= br->len) {
        return 0;
    }
    int bit = (br->data[br->bytePos] >> (7 - br->bitPos)) & 1;
    br->bitPos++;
    if (br->bitPos == 8) {
        br->bitPos = 0;
        br->bytePos++;
    }
    return bit;
}

typedef struct {
    uint32_t firstCode[256];
    int      count[256];
    int      offset[256];
    unsigned char symbols[256];
} HuffmanDecodeTable;

static void huffman_build_decode_table(const uint8_t codeLengths[256], HuffmanDecodeTable* t) {
    int blCount[256] = { 0 };
    for (int s = 0; s < 256; s++) {
        if (codeLengths[s] > 0) { blCount[codeLengths[s]]++; }
    }
    memcpy(t->count, blCount, sizeof(blCount));

    uint32_t code = 0;
    for (int len = 1; len < 256; len++) {
        code = (code + (uint32_t)blCount[len - 1]) << 1;
        t->firstCode[len] = code;
    }

    int offset = 0;
    for (int len = 1; len < 256; len++) {
        t->offset[len] = offset;
        offset += blCount[len];
    }

    int cursor[256];
    memcpy(cursor, t->offset, sizeof(cursor));
    for (int s = 0; s < 256; s++) {
        if (codeLengths[s] > 0) {
            t->symbols[cursor[codeLengths[s]]++] = (unsigned char)s;
        }
    }
}

static bool huffman_decode(BitReader* br, const HuffmanDecodeTable* t, size_t decodedLength, unsigned char* out) {
    for (size_t i = 0; i < decodedLength; i++) {
        uint32_t code = 0;
        int len = 0;
        bool found = false;
        while (len < 255) {
            code = (code << 1) | (uint32_t)bitreader_read_bit(br);
            len++;
            if (t->count[len] > 0) {
                uint32_t idx = code - t->firstCode[len];
                if (idx < (uint32_t)t->count[len]) {
                    out[i] = t->symbols[t->offset[len] + idx];
                    found = true;
                    break;
                }
            }
        }
        if (!found) {
            return false;
        }
    }
    return true;
}

// ---------------------------------------------------------------------
// Unique-color peek. Short-circuits once the count exceeds
// MAX_UNIQUE_COLORS -- no point scanning further once we already know
// the image needs the true-color tier.
// ---------------------------------------------------------------------
static bool color_equal(Color a, Color b) {
    return a.r == b.r && a.g == b.g && a.b == b.b && a.a == b.a;
}

static int find_color_index(const Color* palette, int count, Color c) {
    for (int i = 0; i < count; i++) {
        if (color_equal(palette[i], c)) { return i; }
    }
    return -1;
}

// Returns true if the image fits within MAX_UNIQUE_COLORS; palette/count
// are filled in either way (count capped at MAX_UNIQUE_COLORS+1 as an
// overflow signal if it doesn't fit -- callers check the return value,
// not just the count, to know which case they're in).
static bool peek_unique_colors(const unsigned char* rgba, int w, int h, Color* palette, int* outCount) {
    int count = 0;
    int pixelCount = w * h;
    for (int i = 0; i < pixelCount; i++) {
        Color c = {
            rgba[i * 4 + 0], rgba[i * 4 + 1], rgba[i * 4 + 2], rgba[i * 4 + 3]
        };
        if (find_color_index(palette, count, c) < 0) {
            if (count >= MAX_UNIQUE_COLORS) {
                *outCount = count;
                return false;
            }
            palette[count++] = c;
        }
    }
    *outCount = count;
    return true;
}

// ---------------------------------------------------------------------
// Format selection / validation
// ---------------------------------------------------------------------
typedef enum { FMT_AUTO, FMT_PALETTE_SHARED, FMT_PALETTE_OWN, FMT_RGBA8 } RequestedFormat;

static bool parse_format_name(const char* s, RequestedFormat* out) {
    if (strcmp(s, "palette-shared") == 0) { *out = FMT_PALETTE_SHARED; return true; }
    if (strcmp(s, "palette-own")    == 0) { *out = FMT_PALETTE_OWN;    return true; }
    if (strcmp(s, "rgba8")          == 0) { *out = FMT_RGBA8;          return true; }
    return false;
}

// ---------------------------------------------------------------------
// Per-image packing. Builds one complete blob:
//   uint16 width, uint16 height, uint8 passCount, uint8 passIDs[passCount],
//   [1024-byte palette table -- PALETTE_OWN only],
//   uint32 dataLength, [payload]
// For RLE_RGBA8, payload is 4x uint32 plane lengths followed by the R,G,B,A
// planes' RLE streams in that order (see pixiepackertool.txt).
// ---------------------------------------------------------------------
typedef struct {
    char              name[128];
    ByteBuf           blob;
    int               width, height;
    PixieUnpackFormat format;
    size_t            offsetInMaster;
    // Kept alongside the blob for --preview's use only -- PALETTE_SHARED's
    // blob deliberately never embeds a palette (that's the whole point of
    // "shared": it resolves against whatever GDMF palette slot the game
    // loads at runtime), so there is nowhere else to recover these colors
    // from once packing is done.
    Color             extractedPalette[16];
    int               extractedPaletteCount;
} PackedImage;

static void sanitize_identifier(const char* path, char* out, size_t outCap) {
    // Strip directory + extension, replace anything not [A-Za-z0-9_] with '_'.
    const char* base = strrchr(path, '/');
    const char* base2 = strrchr(path, '\\');
    if (base2 && (!base || base2 > base)) { base = base2; }
    base = base ? base + 1 : path;

    size_t i = 0;
    for (; base[i] && base[i] != '.' && i < outCap - 1; i++) {
        char c = base[i];
        out[i] = (isalnum((unsigned char)c)) ? c : '_';
    }
    out[i] = '\0';
}

static void to_upper(char* s) {
    for (; *s; s++) { *s = (char)toupper((unsigned char)*s); }
}

static bool pack_image(const char* path, RequestedFormat requested, PackedImage* out) {
    int w, h, channels;
    unsigned char* rgba = stbi_load(path, &w, &h, &channels, 4);
    if (!rgba) {
        fprintf(stderr, "PixiePacker: failed to load '%s': %s\n", path, stbi_failure_reason());
        return false;
    }

    Color palette[MAX_UNIQUE_COLORS];
    int   uniqueCount = 0;
    bool  fitsSmallPalette = peek_unique_colors(rgba, w, h, palette, &uniqueCount);

    // Decide the tier: shared (<=16), own (<=256), or true color.
    bool fits16  = fitsSmallPalette && uniqueCount <= 16;
    bool fits256 = fitsSmallPalette; // peek already caps at MAX_UNIQUE_COLORS (256)

    PixieUnpackFormat format;
    if (requested == FMT_AUTO) {
        if (fits16)       { format = PIXIE_FORMAT_RLE_PALETTE_SHARED; }
        else if (fits256) { format = PIXIE_FORMAT_RLE_PALETTE_OWN; }
        else              { format = PIXIE_FORMAT_RLE_RGBA8; }
    } else if (requested == FMT_PALETTE_SHARED) {
        if (!fits16) {
            fprintf(stderr, "PixiePacker: '%s' has %d unique colors, palette-shared allows at most 16\n",
                path, fitsSmallPalette ? uniqueCount : 257);
            free(rgba);
            return false;
        }
        format = PIXIE_FORMAT_RLE_PALETTE_SHARED;
    } else if (requested == FMT_PALETTE_OWN) {
        if (!fits256) {
            fprintf(stderr, "PixiePacker: '%s' has more than 256 unique colors, palette-own allows at most 256\n", path);
            free(rgba);
            return false;
        }
        format = PIXIE_FORMAT_RLE_PALETTE_OWN;
    } else {
        format = PIXIE_FORMAT_RLE_RGBA8;
    }

    int pixelCount = w * h;
    bool isPaletteTier = (format == PIXIE_FORMAT_RLE_PALETTE_SHARED || format == PIXIE_FORMAT_RLE_PALETTE_OWN);

    // Step 1: build the tier's pre-compression stream and RLE-encode it --
    // an index stream for the two palette tiers, or (for RLE_RGBA8) 4
    // independent per-channel RLE streams assembled into one contiguous
    // [4x uint32 plane length][plane0][plane1][plane2][plane3] buffer, so
    // Huffman below can wrap it generically as a single unit regardless
    // of which tier produced it. Auto mode may downgrade `format` to a
    // raw tier if RLE doesn't actually help, same fallback this tool has
    // always done -- just decided before anything is written to the
    // final blob now, rather than needing the blob rebuilt out from under
    // an earlier speculative write.
    ByteBuf rlePayload;
    buf_init(&rlePayload);
    bool rleApplies = false;
    bool usedDelta  = false;  // RLE_RGBA8 only -- see that branch below
    bool usedLZ     = false;  // set below wherever LZ wins over RLE

    if (isPaletteTier) {
        unsigned char* indices = malloc((size_t)pixelCount);
        for (int i = 0; i < pixelCount; i++) {
            Color c = { rgba[i*4], rgba[i*4+1], rgba[i*4+2], rgba[i*4+3] };
            int idx = find_color_index(palette, uniqueCount, c);
            indices[i] = (unsigned char)idx; // guaranteed found -- palette was built from this same image
        }
        rle_encode(indices, (size_t)pixelCount, &rlePayload);

        // LZ is an alternative to RLE, never stacked with it -- try both
        // on the same index stream and keep whichever is smaller.
        ByteBuf lzPayload;
        buf_init(&lzPayload);
        lz_encode(indices, (size_t)pixelCount, &lzPayload);
        if (lzPayload.len < rlePayload.len) {
            free(rlePayload.data);
            rlePayload = lzPayload;
            usedLZ = true;
        } else {
            free(lzPayload.data);
        }

        // Auto mode only: if neither RLE nor LZ actually helped versus
        // the raw uncompressed equivalent at this tier, fall back rather
        // than "compressing" into something bigger. Manual mode proceeds
        // with whatever was explicitly requested regardless.
        size_t rawEquivalent = (format == PIXIE_FORMAT_RLE_PALETTE_SHARED)
            ? (size_t)((pixelCount + 1) / 2)  // PALETTE4BPP: 2 indices/byte
            : (size_t)pixelCount * 4;          // no raw own-256 format -- compare to RGBA8
        if (requested == FMT_AUTO && rlePayload.len >= rawEquivalent) {
            fprintf(stderr, "PixiePacker: '%s' -- RLE/LZ did not help at this tier, falling back to a raw format\n", path);
            format = (format == PIXIE_FORMAT_RLE_PALETTE_SHARED) ? PIXIE_FORMAT_PALETTE4BPP : PIXIE_FORMAT_RGBA8;
            usedLZ = false;
        } else {
            rleApplies = true;
        }
        free(indices);
    } else {
        // RLE_RGBA8: four independent per-channel RLE streams (R,G,B,A),
        // each far more likely to have long runs than 4-byte RGBA tuples
        // would -- e.g. alpha is frequently constant across a whole image
        // even when RGB varies.
        //
        // Two variants are tried per plane -- plain RLE, and DELTA (see
        // delta_encode's comment) applied before RLE -- and whichever
        // variant's assembled total is smaller wins, all 4 planes
        // together. This is a per-format decision, not per-channel:
        // splitting the pass list per-plane would mean 4 separate
        // headers instead of one shared one, more complexity than the
        // marginal gain is worth. Delta is harmless even on a plane it
        // doesn't help (a constant channel's delta stream is just
        // [value, 0, 0, ...], which RLE compresses just as well as the
        // constant channel itself would have), so trying it blanket
        // across all 4 planes doesn't give up anything in that case.
        unsigned char* planes[4];
        for (int c = 0; c < 4; c++) {
            planes[c] = malloc((size_t)pixelCount);
            for (int i = 0; i < pixelCount; i++) { planes[c][i] = rgba[i * 4 + c]; }
        }

        ByteBuf planeRLE[4];
        for (int c = 0; c < 4; c++) {
            buf_init(&planeRLE[c]);
            rle_encode(planes[c], (size_t)pixelCount, &planeRLE[c]);
        }
        size_t totalRLE = planeRLE[0].len + planeRLE[1].len + planeRLE[2].len + planeRLE[3].len;

        // LZ is an alternative to RLE, tried on the same (non-delta)
        // planes -- see the palette-tier branch above for why it's never
        // stacked with RLE.
        ByteBuf planeLZ[4];
        for (int c = 0; c < 4; c++) {
            buf_init(&planeLZ[c]);
            lz_encode(planes[c], (size_t)pixelCount, &planeLZ[c]);
        }
        size_t totalLZ = planeLZ[0].len + planeLZ[1].len + planeLZ[2].len + planeLZ[3].len;

        unsigned char* deltaPlanes[4];
        ByteBuf deltaPlaneRLE[4];
        ByteBuf deltaPlaneLZ[4];
        for (int c = 0; c < 4; c++) {
            deltaPlanes[c] = malloc((size_t)pixelCount);
            delta_encode(planes[c], (size_t)pixelCount, deltaPlanes[c]);
            buf_init(&deltaPlaneRLE[c]);
            rle_encode(deltaPlanes[c], (size_t)pixelCount, &deltaPlaneRLE[c]);
            buf_init(&deltaPlaneLZ[c]);
            lz_encode(deltaPlanes[c], (size_t)pixelCount, &deltaPlaneLZ[c]);
        }
        size_t totalDeltaRLE = deltaPlaneRLE[0].len + deltaPlaneRLE[1].len + deltaPlaneRLE[2].len + deltaPlaneRLE[3].len;
        size_t totalDeltaLZ  = deltaPlaneLZ[0].len  + deltaPlaneLZ[1].len  + deltaPlaneLZ[2].len  + deltaPlaneLZ[3].len;

        // Four candidate pipelines for this format's per-channel planes
        // (plain RLE, plain LZ, delta+RLE, delta+LZ) -- keep whichever
        // assembles smallest, all 4 planes together (see the "per-format,
        // not per-channel" reasoning in the comment above this branch).
        ByteBuf* winningPlaneRLE = planeRLE;
        size_t   winningTotal    = totalRLE;
        bool     deltaWins = false;
        bool     lzWins    = false;

        if (totalLZ < winningTotal) {
            winningPlaneRLE = planeLZ; winningTotal = totalLZ; deltaWins = false; lzWins = true;
        }
        if (totalDeltaRLE < winningTotal) {
            winningPlaneRLE = deltaPlaneRLE; winningTotal = totalDeltaRLE; deltaWins = true; lzWins = false;
        }
        if (totalDeltaLZ < winningTotal) {
            winningPlaneRLE = deltaPlaneLZ; winningTotal = totalDeltaLZ; deltaWins = true; lzWins = true;
        }

        size_t rawEquivalent = (size_t)pixelCount * 4;
        if (requested == FMT_AUTO && winningTotal >= rawEquivalent) {
            fprintf(stderr, "PixiePacker: '%s' -- RLE/LZ did not help, falling back to raw RGBA8\n", path);
            format = PIXIE_FORMAT_RGBA8;
        } else {
            uint32_t lens[4] = {
                (uint32_t)winningPlaneRLE[0].len, (uint32_t)winningPlaneRLE[1].len,
                (uint32_t)winningPlaneRLE[2].len, (uint32_t)winningPlaneRLE[3].len
            };
            buf_push(&rlePayload, lens, sizeof(lens));
            for (int c = 0; c < 4; c++) { buf_push(&rlePayload, winningPlaneRLE[c].data, winningPlaneRLE[c].len); }
            rleApplies = true;
            usedDelta  = deltaWins;
            usedLZ     = lzWins;
        }

        for (int c = 0; c < 4; c++) {
            free(planes[c]);
            free(planeRLE[c].data);
            free(planeLZ[c].data);
            free(deltaPlanes[c]);
            free(deltaPlaneRLE[c].data);
            free(deltaPlaneLZ[c].data);
        }
    }

    // Step 2: if RLE survived, see if Huffman-wrapping its output shrinks
    // things further -- same "try it, keep it only if it actually helps"
    // pattern as RLE's own fallback above. Huffman is generic to any byte
    // stream, so this applies identically whether rlePayload is an
    // index-tier RLE stream or RLE_RGBA8's assembled 4-plane blob; it
    // doesn't need to know which. +4 on the RLE side accounts for the
    // plain uint32 dataLength prefix an RLE-only payload needs that a
    // Huffman-wrapped one doesn't (Huffman's own header is already
    // self-describing -- see huffman_encode).
    bool useHuffman = false;
    ByteBuf huffmanPayload;
    buf_init(&huffmanPayload);
    if (rleApplies) {
        huffman_encode(rlePayload.data, rlePayload.len, &huffmanPayload);
        if (huffmanPayload.len < rlePayload.len + 4) {
            useHuffman = true;
        }
    }

    // Step 3: format/rleApplies/usedDelta/usedLZ/useHuffman are all
    // finalized -- build the actual blob exactly once, no speculative
    // writes or rebuild-from-scratch needed. DELTA always comes first in
    // the pass list (it's the first transform applied, at the raw-plane
    // level), the primary pass (RLE or its alternative LZ) is always
    // second, and HUFFMAN always comes last (it wraps whatever came
    // before it) -- matches gdmf_pixies.c's pixie_unpack_blob validation
    // exactly.
    uint8_t primaryPass = usedLZ ? PIXIE_PASS_LZ : PIXIE_PASS_RLE;

    ByteBuf blob;
    buf_init(&blob);
    buf_push_u16(&blob, (uint16_t)w);
    buf_push_u16(&blob, (uint16_t)h);
    if (!rleApplies) {
        buf_push_u8(&blob, 0);  // passCount -- raw, no passes applied
    } else if (usedDelta && useHuffman) {
        buf_push_u8(&blob, 3);
        buf_push_u8(&blob, PIXIE_PASS_DELTA);
        buf_push_u8(&blob, primaryPass);
        buf_push_u8(&blob, PIXIE_PASS_HUFFMAN);
    } else if (usedDelta) {
        buf_push_u8(&blob, 2);
        buf_push_u8(&blob, PIXIE_PASS_DELTA);
        buf_push_u8(&blob, primaryPass);
    } else if (useHuffman) {
        buf_push_u8(&blob, 2);
        buf_push_u8(&blob, primaryPass);
        buf_push_u8(&blob, PIXIE_PASS_HUFFMAN);
    } else {
        buf_push_u8(&blob, 1);
        buf_push_u8(&blob, primaryPass);
    }

    if (format == PIXIE_FORMAT_RLE_PALETTE_OWN) {
        // Embed the full 256-entry palette table, zero-padded past
        // however many colors this image actually uses.
        Color full[256] = { 0 };
        memcpy(full, palette, (size_t)uniqueCount * sizeof(Color));
        buf_push(&blob, full, sizeof(full));
    }

    if (!rleApplies) {
        if (format == PIXIE_FORMAT_PALETTE4BPP) {
            unsigned char* packed4 = calloc((size_t)(pixelCount + 1) / 2, 1);
            for (int i = 0; i < pixelCount; i++) {
                Color c = { rgba[i*4], rgba[i*4+1], rgba[i*4+2], rgba[i*4+3] };
                int idx = find_color_index(palette, uniqueCount, c);
                unsigned char nibble = (unsigned char)(idx & 0x0F);
                if (i % 2 == 0) { packed4[i/2] |= (unsigned char)(nibble << 4); }
                else            { packed4[i/2] |= nibble; }
            }
            buf_push_u32(&blob, (uint32_t)((pixelCount + 1) / 2));
            buf_push(&blob, packed4, (size_t)(pixelCount + 1) / 2);
            free(packed4);
        } else { // PIXIE_FORMAT_RGBA8
            buf_push_u32(&blob, (uint32_t)pixelCount * 4);
            buf_push(&blob, rgba, (size_t)pixelCount * 4);
        }
    } else if (useHuffman) {
        buf_push(&blob, huffmanPayload.data, huffmanPayload.len);
    } else {
        buf_push_u32(&blob, (uint32_t)rlePayload.len);
        buf_push(&blob, rlePayload.data, rlePayload.len);
    }

    free(rlePayload.data);
    free(huffmanPayload.data);

    sanitize_identifier(path, out->name, sizeof(out->name));
    out->blob   = blob;
    out->width  = w;
    out->height = h;
    out->format = format;
    // Only meaningful (and only ever needed, by --preview) for the two
    // 16-color-tier formats, which never embed their palette in the blob
    // itself. Harmless to leave zeroed for the other tiers.
    if (format == PIXIE_FORMAT_RLE_PALETTE_SHARED || format == PIXIE_FORMAT_PALETTE4BPP) {
        out->extractedPaletteCount = uniqueCount;
        memcpy(out->extractedPalette, palette, (size_t)uniqueCount * sizeof(Color));
    } else {
        out->extractedPaletteCount = 0;
    }

    free(rgba);
    return true;
}

// ---------------------------------------------------------------------
// --preview: decode a packed blob back into an RGBA buffer and write it
// as a PNG, purely for visually verifying the packer/decoder logic
// without needing to run the engine. Mirrors (in spirit) what
// PIXIE_OP_UNPACK will do on the engine side.
// ---------------------------------------------------------------------
// Dispatches on img->format directly rather than branching on passCount --
// each format value only ever appears with one specific passCount in
// practice (the two RLE palette formats and RLE_RGBA8 are always
// passCount 1; PALETTE4BPP and RGBA8 only ever arise as an auto-mode
// fallback and are always passCount 0), but keying off format keeps every
// case unambiguous and keeps PALETTE4BPP from being silently misread as
// if it were RGBA8-shaped data (an earlier version of this function did
// exactly that).
// Reads the (possibly Huffman-wrapped) RLE payload starting at `*pp`,
// advancing `*pp` past everything consumed -- the tool-side mirror of
// GDMF/gdmf_pixies.c's pixie_undo_huffman_if_present, minus that side's
// RAM-bounds checks (this operates on the tool's own trusted in-memory
// blob). `*outOwned` tells the caller whether the returned pointer is a
// freshly decoded heap buffer (free it) or a pointer straight into the
// blob (don't).
static const unsigned char* preview_undo_huffman(const unsigned char** pp, bool useHuffman, size_t* outLen, bool* outOwned) {
    const unsigned char* p = *pp;
    if (!useHuffman) {
        uint32_t len = *(const uint32_t*)p; p += 4;
        const unsigned char* data = p;
        p += len;
        *pp = p;
        *outLen = len;
        *outOwned = false;
        return data;
    }

    uint8_t codeLengths[256];
    memcpy(codeLengths, p, 256);
    p += 256;
    uint32_t decodedLength = *(const uint32_t*)p; p += 4;
    uint32_t encodedLength = *(const uint32_t*)p; p += 4;

    HuffmanDecodeTable table;
    huffman_build_decode_table(codeLengths, &table);

    unsigned char* decoded = malloc(decodedLength);
    BitReader br = { p, encodedLength, 0, 0 };
    huffman_decode(&br, &table, decodedLength, decoded);
    p += encodedLength;

    *pp = p;
    *outLen = decodedLength;
    *outOwned = true;
    return decoded;
}

// Decodes whichever "primary" pass produced `data` -- RLE or its
// alternative LZ -- mirroring gdmf_pixies.c's pixie_decode_primary.
static void preview_decode_primary(bool useLZ, const unsigned char* data, size_t len, unsigned char* out) {
    if (useLZ) {
        lz_decode(data, len, out);
    } else {
        rle_decode(data, len, out);
    }
}

static void preview_image(const PackedImage* img, const char* outPath) {
    const unsigned char* p = img->blob.data;
    uint16_t w = *(const uint16_t*)p; p += 2;
    uint16_t h = *(const uint16_t*)p; p += 2;
    uint8_t passCount = *p; p += 1;
    // passCount 2 is ambiguous by count alone -- {RLE/LZ, HUFFMAN} and
    // {DELTA, RLE/LZ} are both 2 passes -- so the actual pass IDs have to
    // be inspected, same as gdmf_pixies.c's pixie_unpack_blob does.
    bool useDelta   = false;
    bool useHuffman = false;
    bool useLZ      = false;
    if (passCount == 1) {
        useLZ = (p[0] == PIXIE_PASS_LZ);
    } else if (passCount == 2) {
        if (p[0] == PIXIE_PASS_DELTA) {
            useDelta = true;
            useLZ    = (p[1] == PIXIE_PASS_LZ);
        } else {
            useLZ      = (p[0] == PIXIE_PASS_LZ);
            useHuffman = true;
        }
    } else if (passCount == 3) {
        useDelta   = true;
        useLZ      = (p[1] == PIXIE_PASS_LZ);
        useHuffman = true;
    }
    p += passCount; // skip pass ID list

    int pixelCount = w * h;
    unsigned char* rgba = malloc((size_t)pixelCount * 4);

    switch (img->format) {
        case PIXIE_FORMAT_RLE_PALETTE_OWN: {
            const Color* palette = (const Color*)p;
            p += 256 * sizeof(Color);
            size_t rleLen; bool owned;
            const unsigned char* rleData = preview_undo_huffman(&p, useHuffman, &rleLen, &owned);
            unsigned char* indices = malloc((size_t)pixelCount);
            preview_decode_primary(useLZ, rleData, rleLen, indices);
            if (owned) { free((void*)rleData); }
            for (int i = 0; i < pixelCount; i++) {
                Color c = palette[indices[i]];
                rgba[i*4+0] = c.r; rgba[i*4+1] = c.g; rgba[i*4+2] = c.b; rgba[i*4+3] = c.a;
            }
            free(indices);
            break;
        }
        case PIXIE_FORMAT_RLE_PALETTE_SHARED: {
            // No embedded palette in this format's blob at all -- reuse
            // this image's own extractedPalette (kept alongside the blob
            // specifically for this) rather than trying to read one out
            // of the packed data, since there is none there to read.
            size_t rleLen; bool owned;
            const unsigned char* rleData = preview_undo_huffman(&p, useHuffman, &rleLen, &owned);
            unsigned char* indices = malloc((size_t)pixelCount);
            preview_decode_primary(useLZ, rleData, rleLen, indices);
            if (owned) { free((void*)rleData); }
            for (int i = 0; i < pixelCount; i++) {
                Color c = img->extractedPalette[indices[i]];
                rgba[i*4+0] = c.r; rgba[i*4+1] = c.g; rgba[i*4+2] = c.b; rgba[i*4+3] = c.a;
            }
            free(indices);
            break;
        }
        case PIXIE_FORMAT_PALETTE4BPP: {
            // Raw fallback tier -- always passCount 0, never Huffman-wrapped.
            uint32_t dataLength = *(const uint32_t*)p; p += 4;
            (void)dataLength;
            for (int i = 0; i < pixelCount; i++) {
                unsigned char byte = p[i / 2];
                unsigned char idx = (i % 2 == 0) ? (unsigned char)(byte >> 4) : (unsigned char)(byte & 0x0F);
                Color c = img->extractedPalette[idx];
                rgba[i*4+0] = c.r; rgba[i*4+1] = c.g; rgba[i*4+2] = c.b; rgba[i*4+3] = c.a;
            }
            break;
        }
        case PIXIE_FORMAT_RLE_RGBA8: {
            size_t rleLen; bool owned;
            const unsigned char* rleData = preview_undo_huffman(&p, useHuffman, &rleLen, &owned);
            const uint32_t* lens = (const uint32_t*)rleData;
            const unsigned char* planeData = rleData + 16;
            unsigned char* planes[4];
            size_t offset = 0;
            for (int c = 0; c < 4; c++) {
                planes[c] = malloc((size_t)pixelCount);
                preview_decode_primary(useLZ, planeData + offset, lens[c], planes[c]);
                if (useDelta) {
                    unsigned char* undelta = malloc((size_t)pixelCount);
                    delta_decode(planes[c], (size_t)pixelCount, undelta);
                    free(planes[c]);
                    planes[c] = undelta;
                }
                offset += lens[c];
            }
            for (int i = 0; i < pixelCount; i++) {
                rgba[i*4+0] = planes[0][i]; rgba[i*4+1] = planes[1][i];
                rgba[i*4+2] = planes[2][i]; rgba[i*4+3] = planes[3][i];
            }
            for (int c = 0; c < 4; c++) { free(planes[c]); }
            if (owned) { free((void*)rleData); }
            break;
        }
        case PIXIE_FORMAT_RGBA8: {
            // Raw fallback tier -- always passCount 0, never Huffman-wrapped.
            uint32_t dataLength = *(const uint32_t*)p; p += 4;
            memcpy(rgba, p, dataLength);
            break;
        }
        default:
            memset(rgba, 0, (size_t)pixelCount * 4);
            break;
    }

    stbi_write_png(outPath, w, h, 4, rgba, w * 4);
    free(rgba);
}

// ---------------------------------------------------------------------
// Header emission -- matches the project's existing generated-asset
// convention (see ASSETS/tile_heart.h): unsigned char array literal,
// wrapped at a fixed line width, plus a Color palette array where
// applicable.
// ---------------------------------------------------------------------
static void write_byte_array(FILE* f, const char* name, const unsigned char* data, size_t len) {
    fprintf(f, "unsigned char %s[%zu] = {\n    ", name, len);
    for (size_t i = 0; i < len; i++) {
        fprintf(f, "0x%02x, ", data[i]);
        if ((i + 1) % 16 == 0) { fprintf(f, "\n    "); }
    }
    fprintf(f, "\n};\n\n");
}

static void write_palette_array(FILE* f, const char* name, const Color* palette, int count) {
    fprintf(f, "Color %s[%d] = {\n    ", name, count);
    for (int i = 0; i < count; i++) {
        fprintf(f, "{%u,%u,%u,%u}, ", palette[i].r, palette[i].g, palette[i].b, palette[i].a);
        if ((i + 1) % 4 == 0) { fprintf(f, "\n    "); }
    }
    fprintf(f, "\n};\n\n");
}

static const char* format_enum_name(PixieUnpackFormat fmt) {
    switch (fmt) {
        case PIXIE_FORMAT_RGBA8:              return "PIXIE_FORMAT_RGBA8";
        case PIXIE_FORMAT_PALETTE4BPP:         return "PIXIE_FORMAT_PALETTE4BPP";
        case PIXIE_FORMAT_RLE_PALETTE_SHARED:  return "PIXIE_FORMAT_RLE_PALETTE_SHARED";
        case PIXIE_FORMAT_RLE_PALETTE_OWN:     return "PIXIE_FORMAT_RLE_PALETTE_OWN";
        case PIXIE_FORMAT_RLE_RGBA8:           return "PIXIE_FORMAT_RLE_RGBA8";
        default:                               return "PIXIE_FORMAT_UNKNOWN";
    }
}

static void write_header(const char* outName, PackedImage* images, int imageCount) {
    // Concatenate every image's blob into one master buffer, recording
    // each one's offset -- this is the "20 faces in one PixieWrite()"
    // layout described in pixiepackertool.txt.
    ByteBuf master;
    buf_init(&master);
    for (int i = 0; i < imageCount; i++) {
        images[i].offsetInMaster = master.len;
        buf_push(&master, images[i].blob.data, images[i].blob.len);
    }

    char path[256];
    snprintf(path, sizeof(path), "%s.h", outName);
    FILE* f = fopen(path, "w");
    if (!f) {
        fprintf(stderr, "PixiePacker: failed to open '%s' for writing\n", path);
        return;
    }

    fprintf(f, "// Generated by PixiePacker -- do not edit by hand.\n");
    fprintf(f, "// See tools/PixiePacker/pixiepackertool.txt for the packed format this data uses.\n\n");

    char arrayName[160];
    snprintf(arrayName, sizeof(arrayName), "%s_pixie_data", outName);
    write_byte_array(f, arrayName, master.data, master.len);

    for (int i = 0; i < imageCount; i++) {
        char upperName[128];
        strncpy(upperName, images[i].name, sizeof(upperName) - 1);
        upperName[sizeof(upperName) - 1] = '\0';
        to_upper(upperName);

        fprintf(f, "#define %s_OFFSET        %zu\n", upperName, images[i].offsetInMaster);
        fprintf(f, "#define %s_PIXIE_FORMAT  %s\n", upperName, format_enum_name(images[i].format));
        fprintf(f, "#define %s_PIXIE_WIDTH   %d\n", upperName, images[i].width);
        fprintf(f, "#define %s_PIXIE_HEIGHT  %d\n\n", upperName, images[i].height);

        // RLE_PALETTE_SHARED's blob deliberately never embeds a palette
        // (it resolves against whatever GDMF palette slot the game loads
        // at runtime) -- so this is the only way the game finds out which
        // 16 colors to SetPalette() before calling UNPACK. PIXIE_FORMAT_
        // PALETTE4BPP (the raw auto-mode fallback for the 16-color tier)
        // resolves against a palette slot the exact same way and needs
        // this just as much -- it was missed here originally, which left
        // any image that happens to fall back to this format (e.g. a
        // 16-color image where RLE doesn't actually help) with no way to
        // know what colors to load at all. RLE_PALETTE_OWN is the only
        // one that genuinely doesn't need this: its palette already
        // travels inside the blob itself, so emitting it here too would
        // be redundant.
        if (images[i].format == PIXIE_FORMAT_RLE_PALETTE_SHARED || images[i].format == PIXIE_FORMAT_PALETTE4BPP) {
            char paletteName[160];
            snprintf(paletteName, sizeof(paletteName), "%s_pixie_palette", images[i].name);
            write_palette_array(f, paletteName, images[i].extractedPalette, images[i].extractedPaletteCount);
            fprintf(f, "#define %s_PIXIE_PALETTE_COUNT %d\n\n", upperName, images[i].extractedPaletteCount);
        }
    }

    fclose(f);
    free(master.data);

    printf("PixiePacker: wrote %s (%zu bytes packed, %d image(s))\n", path, master.len, imageCount);
}

int main(int argc, char** argv) {
    if (argc < 2) {
        fprintf(stderr, "Usage: pixiepacker <input.png> [more.png ...] --output=<name> [--format=<format>] [--preview]\n");
        fprintf(stderr, "  <format> one of: palette-shared, palette-own, rgba8 (default: auto)\n");
        return 1;
    }

    const char* inputs[MAX_IMAGES_PER_RUN];
    int inputCount = 0;
    const char* outputName = NULL;
    RequestedFormat requested = FMT_AUTO;
    bool doPreview = false;

    for (int i = 1; i < argc; i++) {
        if (strncmp(argv[i], "--output=", 9) == 0) {
            outputName = argv[i] + 9;
        } else if (strncmp(argv[i], "--format=", 9) == 0) {
            if (!parse_format_name(argv[i] + 9, &requested)) {
                fprintf(stderr, "PixiePacker: unknown format '%s'\n", argv[i] + 9);
                return 1;
            }
        } else if (strcmp(argv[i], "--preview") == 0) {
            doPreview = true;
        } else if (inputCount < MAX_IMAGES_PER_RUN) {
            inputs[inputCount++] = argv[i];
        }
    }

    if (inputCount == 0) {
        fprintf(stderr, "PixiePacker: no input PNG(s) given\n");
        return 1;
    }

    char defaultName[128];
    if (!outputName) {
        sanitize_identifier(inputs[0], defaultName, sizeof(defaultName));
        outputName = defaultName;
    }

    PackedImage images[MAX_IMAGES_PER_RUN];
    int packedCount = 0;
    for (int i = 0; i < inputCount; i++) {
        if (!pack_image(inputs[i], requested, &images[packedCount])) {
            return 1;
        }
        printf("PixiePacker: '%s' -> %s (%dx%d, %zu bytes)\n",
            inputs[i], format_enum_name(images[packedCount].format),
            images[packedCount].width, images[packedCount].height, images[packedCount].blob.len);
        packedCount++;
    }

    write_header(outputName, images, packedCount);

    if (doPreview) {
        for (int i = 0; i < packedCount; i++) {
            char previewPath[300];
            snprintf(previewPath, sizeof(previewPath), "%s_preview.png", images[i].name);
            preview_image(&images[i], previewPath);
            printf("PixiePacker: preview written to %s\n", previewPath);
        }
    }

    for (int i = 0; i < packedCount; i++) { free(images[i].blob.data); }

    return 0;
}
