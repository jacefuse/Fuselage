#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char *argv[]) {
    if (argc != 3) {
        fprintf(stderr, "usage: spv2h <input.spv> <output.h>\n");
        return 1;
    }

    FILE *in = fopen(argv[1], "rb");
    if (!in) { perror(argv[1]); return 1; }

    fseek(in, 0, SEEK_END);
    long size = ftell(in);
    rewind(in);

    unsigned char *data = malloc((size_t)size);
    if (!data) {
        fprintf(stderr, "spv2h: out of memory\n");
        fclose(in);
        return 1;
    }
    if (fread(data, 1, (size_t)size, in) != (size_t)size) {
        fprintf(stderr, "spv2h: read error\n");
        free(data);
        fclose(in);
        return 1;
    }
    fclose(in);

    /* Derive variable name from basename: tile_vert.spv -> tile_vert_spv */
    const char *base = argv[1];
    const char *p;
    for (p = argv[1]; *p; p++)
        if (*p == '/' || *p == '\\') base = p + 1;

    char varname[256];
    size_t vlen = 0;
    for (p = base; *p && vlen < sizeof(varname) - 1; p++, vlen++)
        varname[vlen] = (*p == '.') ? '_' : *p;
    varname[vlen] = '\0';

    FILE *out = fopen(argv[2], "w");
    if (!out) { perror(argv[2]); free(data); return 1; }

    fprintf(out, "unsigned char %s[] = {\n", varname);
    for (long i = 0; i < size; i++) {
        if (i % 12 == 0) fprintf(out, "  ");
        fprintf(out, "0x%02x,", data[i]);
        if ((i + 1) % 12 == 0 || i == size - 1)
            fprintf(out, "\n");
        else
            fprintf(out, " ");
    }
    fprintf(out, "};\n");
    fprintf(out, "unsigned int %s_len = %u;\n", varname, (unsigned int)size);

    fclose(out);
    free(data);
    return 0;
}
