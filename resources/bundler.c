#include <stdio.h>

#define ARGS_SHIFT() \
    {                \
        argc--;      \
        argv++;      \
    }                \
    while (0)

int main(int argc, char* argv[]) {
    if (argc < 3) {
        fprintf(stderr, "Usage: %s <output file> files...\n", argv[0]);
        return 1;
    }

    ARGS_SHIFT();

    FILE* out = fopen(argv[0], "w");
    if (!out) {
        fprintf(stderr, "Failed to open %s to write.\n", argv[1]);
        return 1;
    }

    ARGS_SHIFT();

    fprintf(out, "#ifndef BUNDLE_H\n");
    fprintf(out, "#define BUNDLE_H\n\n");

    for (int i = 0; i < argc; i++) {
        FILE* fp = fopen(argv[i], "r");
        if (!fp) {
            fprintf(stderr, "Failed to open %s to read.\n", argv[i]);
            return 1;
        }

        fprintf(out, "const char bundle%d[] = {", i);

        int index = 0;

        int byte;
        while ((byte = fgetc(fp)) != EOF) {
            if (index % 10 == 0) {
                fprintf(out, "\n    ");
            }
            fprintf(out, "0x%02X, ", byte);
            index++;
        }

        fprintf(out, "\n};\n\n");
        fclose(fp);
    }

    fprintf(out, "const char* bundle[] = {\n");
    for (int i = 0; i < argc; i++) {
        fprintf(out, "    bundle%d,\n", i);
    }
    fprintf(out, "};\n\n");

    fprintf(out, "#endif\n");
    fclose(out);
    return 0;
}
