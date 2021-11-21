#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>
#include <math.h>
#include <string.h>

typedef struct {
    char chunkID[4];
    uint32_t chunkSize;
    uint16_t audioFormat;
    uint16_t numChannels;
    uint32_t sampleRate;
    uint32_t byteRate;
    uint16_t blockAlign;
    uint16_t bitsPerSample;
} FmtChunk;

typedef struct {
    char chunkID[4];
    uint32_t chunkSize;
    uint8_t *data;
} DataChunk;

typedef struct {
    char chunkID[4];
    uint32_t chunkSize;
    char format[4];
    FmtChunk fmt;
    DataChunk data; // TODO: multiple data chunks
} WavFile;

#define READ_VAL(v) nread = fread(&v, sizeof v, 1, file)

WavFile loadWavFile(FILE *file) {
    WavFile wavFile;
    size_t nread;

    // header info
    READ_VAL(wavFile.chunkID);
    READ_VAL(wavFile.chunkSize);
    READ_VAL(wavFile.format);

    // fmt chunk
    READ_VAL(wavFile.fmt.chunkID);
    READ_VAL(wavFile.fmt.chunkSize);
    READ_VAL(wavFile.fmt.audioFormat);
    READ_VAL(wavFile.fmt.numChannels);
    READ_VAL(wavFile.fmt.sampleRate);
    READ_VAL(wavFile.fmt.byteRate);
    READ_VAL(wavFile.fmt.blockAlign);
    READ_VAL(wavFile.fmt.bitsPerSample);

    // data chunk
    READ_VAL(wavFile.data.chunkID);
    READ_VAL(wavFile.data.chunkSize);
    wavFile.data.data = malloc(wavFile.data.chunkSize);
    nread = fread(wavFile.data.data, sizeof(uint8_t), wavFile.data.chunkSize, file);

    return wavFile;
}

enum {
    PARITY_NONE,
    PARITY_ODD,
    PARITY_EVEN
} typedef ParityMode;

uint8_t cycleLength = 18;

uint8_t count_bits(uint8_t num) {
    return ((num >> 0) & 1)
         + ((num >> 1) & 1)
         + ((num >> 2) & 1)
         + ((num >> 3) & 1)
         + ((num >> 4) & 1)
         + ((num >> 5) & 1)
         + ((num >> 6) & 1)
         + ((num >> 7) & 1);
}

bool get_bit(WavFile *wavFile, uint8_t cyclesPerBit, uint64_t *frameIndex) {
    uint32_t bit_length = cyclesPerBit * cycleLength;

    uint32_t cycles = 0;
    bool top = false;
    uint32_t top_val = 255;
    uint64_t local_index = 0;

    while (local_index < bit_length) {
        uint8_t x = wavFile->data.data[(*frameIndex) + local_index];
        local_index += 1;

        if (x == top_val) top = true;

        if (!x && top) {
            cycles += 1;
            top = false;
        }
    }

    (*frameIndex) += local_index;
    return cycles == cyclesPerBit * 2;
}

struct {
    size_t size;
    uint8_t *data;
} typedef ByteBuffer;

struct {
    uint64_t parity_errors;
    ByteBuffer data;
} typedef DecodedKCS;

DecodedKCS decode_kcs(
    WavFile wavFile, uint32_t baud, uint8_t data_bits, uint8_t stop_bits,
    uint8_t start_bits, ParityMode parity_mode, uint16_t leader
) {
    uint16_t cycles_per_bit = 1200 / baud;
    uint64_t parity_errors = 0;

    uint64_t byteIndex = 0;
    
    ByteBuffer buffer;
    buffer.size = 1024;
    buffer.data = malloc(1024);

    uint8_t bits_done;
    uint8_t num;

    uint64_t frames = wavFile.data.chunkSize;
    uint64_t frameIndex = 0;

    if (wavFile.fmt.sampleRate != 22050) {
        
    }
    else {
        while (true) {
            bool bit = get_bit(&wavFile, 1, &frameIndex);
            if (!bit) {
                frameIndex -= cycleLength;
                break;
            }
            else {
                frames -= cycleLength;
            }
        }

        while (frameIndex < frames) {
            if (bits_done == 0) {
                for (uint16_t i = 0; i < start_bits; i++) {
                    get_bit(&wavFile, cycles_per_bit, &frameIndex);
                }
            }

            bool bit = get_bit(&wavFile, cycles_per_bit, &frameIndex);
            num += bit << bits_done;

            bits_done += 1;

            if (bits_done == data_bits) {
                buffer.data[byteIndex] = num;
                byteIndex += 1;

                // handle if buffer is full
                if (byteIndex > buffer.size) {
                    buffer.size += 1024;
                    buffer.data = realloc(buffer.data, buffer.size);
                }

                // parity checks
                if (parity_mode == PARITY_ODD) {
                    bool p = get_bit(&wavFile, cycles_per_bit, &frameIndex);
                    uint8_t bit_count = count_bits(num) + p;
                    if (bit_count % 2 != 1)
                        parity_errors += 1;
                }

                if (parity_mode == PARITY_EVEN) {
                    bool p = get_bit(&wavFile, cycles_per_bit, &frameIndex);
                    uint8_t bit_count = count_bits(num) + p;
                    if (bit_count % 2 != 0)
                        parity_errors += 1;
                }

                num = 0;
                bits_done = 0;

                for (uint8_t i = 0; i < stop_bits; i++) {
                    get_bit(&wavFile, cycles_per_bit, &frameIndex);
                }
            }
        }
    }
    // resize buffer to actual size
    buffer.size = byteIndex;

    DecodedKCS decoded = { parity_errors, buffer };

    return decoded;
}

#undef READ_VAL

struct {
    uint32_t baud;
    uint8_t data_bits;
    uint8_t stop_bits;
    uint8_t start_bits;
    ParityMode parity_mode;
    uint16_t leader;
    bool makeWaveFile;
    bool consoleOutput;

} typedef KCS_Config;

const char *info = "KCS Version 0.1  21-Nov-2021\n"
"Use: KCS [options] infile [outfile]\n\n"
"-Bn baud  1=1600 2=1200  -O  odd parity\n"
"-Ln leader (sec)         -E  even parity\n"
"-C  console output       -D  7 data bits\n"
"-M  make wavefile        -S  1 stop bit\n"
"Default: decode WAV file, 300 baud, 8 data, 2 stop bits, no parity\n\n"
"Conversion utility for Kansas City Standard tapes.\n"
"Accepts 8-bit mono WAV wavefiles at 22,050 samples per second\n"
"Written by Conqu3red.";

bool prefix(const char *pre, const char *str)
{
    return strncmp(pre, str, strlen(pre)) == 0;
}

int getOptions(int argc, const char *const argv[], KCS_Config *config, char * *infile, char * *outfile) {
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-C") == 0) config->consoleOutput = true;
        else if (strcmp(argv[i], "-M") == 0) config->makeWaveFile = true;
        else if (strcmp(argv[i], "-O") == 0) config->parity_mode = PARITY_ODD;
        else if (strcmp(argv[i], "-E") == 0) config->parity_mode = PARITY_EVEN;
        else if (strcmp(argv[i], "-D") == 0) config->data_bits = 7;
        else if (strcmp(argv[i], "-S") == 0) config->stop_bits = 1;
        else if (prefix("-B", argv[i])) {
            int n = atoi(argv[i] + 2);
            if (n != 1 && n != 2) {
                printf("invalid option ... aborting\n");
                return EXIT_FAILURE;
            }

            config->baud = 600 * n;
        }
        else if (prefix("-L", argv[i])) {
            int n = atoi(argv[i] + 2);
            if (n < 0 || n >= (1 << 16)) {
                printf("invalid option ... aborting\n");
                return EXIT_FAILURE;
            }

            config->leader = n;
        }

        else {
            if (*infile == NULL) {
                *infile = malloc(strlen(argv[i]) + 1);
                strcpy(*infile, argv[i]);
            }
            else {
                *outfile = malloc(strlen(argv[i]) + 1);
                strcpy(*outfile, argv[i]);
            };
        }
    }

    return EXIT_SUCCESS;
}

int handleOptions(KCS_Config config, char * *infile, char * *outfile) {
    if (*outfile == NULL) {
        // TODO: generate outfile name based on infile and mode
        const char *temp = "test.txt";
        *outfile = malloc(strlen(temp) + 1);
        strcpy(*outfile, temp);
    }

    printf("INFILE:  %s\n", *infile);
    printf("OUTFILE:  %s\n", *outfile);
    
    if (config.makeWaveFile) {
        // TODO: make wavefiles
    }
    else {
        FILE *file = fopen(*infile, "r");

        if (file == NULL) {
            printf("File not found.\n");
            return EXIT_FAILURE;
        }

        WavFile wavFile = loadWavFile(file);

        //printf("format: %.4s, %.4s\n", wavFile.chunkID, wavFile.format);
        //printf("ChunkSize: %d\n", wavFile.chunkSize);
        //printf("Sample Rate: %dhz\n", wavFile.fmt.sampleRate);

        fclose(file);

        DecodedKCS decodedKCS = decode_kcs(
            wavFile, config.baud, config.data_bits, config.stop_bits, 1, config.parity_mode, config.leader
        );
        ByteBuffer buf = decodedKCS.data;

        //printf("Decoded Size: %d\n", buf.size);
        if (config.consoleOutput) {
            printf("%.*s\n", buf.size, buf.data);
        }
        else {
            // save to outfile
            FILE *f = fopen(*outfile, "wb");
            fwrite(buf.data, buf.size, 1, f);
            fclose(f);
        }

        printf("%d bytes decoded", buf.size);
        if (config.parity_mode != PARITY_NONE) {
            printf(", %d parity errors.", decodedKCS.parity_errors);
        }
        printf("\ndone\n");

        free(buf.data);
        free(wavFile.data.data);
    }

    return EXIT_SUCCESS;
}

int main(int argc, const char *const argv[]) {
    if (argc == 1) {
        printf("%s", info);
        return EXIT_SUCCESS;
    }

    KCS_Config config = {300, 8, 2, 1, PARITY_NONE, 0, false, false};
    
    char *infile = NULL;
    char *outfile = NULL;

    // TODO: these buffers are broken??

    int exit_code = getOptions(argc, argv, &config, &infile, &outfile);
    if (exit_code != EXIT_SUCCESS) {
        if (infile) free(infile);
        if (outfile) free(outfile);
        return exit_code;
    }

    if (infile == NULL) {
        if (outfile) free(outfile);
        printf("%s", info);
        return EXIT_SUCCESS;
    }

    exit_code = handleOptions(config, &infile, &outfile);
    
    if (infile) free(infile);
    if (outfile) free(outfile);

    return exit_code;
}