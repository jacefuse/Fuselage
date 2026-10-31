/* Fetch-time expansion. SEE: (VIVIAN.md, Compressed instructions) */
#ifndef VIVIAN_COMPRESSED_INTERNAL_H
#define VIVIAN_COMPRESSED_INTERNAL_H
#include <stdint.h>

#define VIVIAN_OPCODE_IMMEDIATE 0x13u
#define VIVIAN_OPCODE_REGISTER 0x33u
#define VIVIAN_OPCODE_LOAD 0x03u
#define VIVIAN_OPCODE_STORE 0x23u
#define VIVIAN_OPCODE_FLOAT_LOAD 0x07u
#define VIVIAN_OPCODE_FLOAT_STORE 0x27u
#define VIVIAN_OPCODE_UPPER_IMMEDIATE 0x37u
#define VIVIAN_OPCODE_JUMP 0x6Fu
#define VIVIAN_OPCODE_REGISTER_JUMP 0x67u
#define VIVIAN_OPCODE_BRANCH 0x63u
#define VIVIAN_OPCODE_SYSTEM 0x73u

/* Inclusive bit slice of the instruction. */
static inline uint32_t vivian_compressed_bits(uint16_t instruction, int high_bit, int low_bit) {
    return ((uint32_t)instruction >> low_bit) & ((1u << (high_bit - low_bit + 1)) - 1u);
}
static inline int32_t vivian_compressed_sign_extend(uint32_t value, int bit_count) {
    uint32_t sign_bit = 1u << (bit_count - 1);
    return (int32_t)((value ^ sign_bit) - sign_bit);
}
/* Three-bit compact register fields select x8 through x15. */
static inline uint32_t vivian_compressed_register(uint16_t instruction, int low_bit) {
    return vivian_compressed_bits(instruction, low_bit + 2, low_bit) + 8u;
}

static inline uint32_t vivian_encode_register_instruction(uint32_t function7, uint32_t source_register2,
                                                          uint32_t source_register1, uint32_t function3,
                                                          uint32_t destination_register, uint32_t opcode) {
    return (function7 << 25) | (source_register2 << 20) | (source_register1 << 15) | (function3 << 12) |
           (destination_register << 7) | opcode;
}
static inline uint32_t vivian_encode_immediate_instruction(int32_t immediate, uint32_t source_register1,
                                                           uint32_t function3, uint32_t destination_register,
                                                           uint32_t opcode) {
    return (((uint32_t)immediate & 0xFFFu) << 20) | (source_register1 << 15) | (function3 << 12) |
           (destination_register << 7) | opcode;
}
static inline uint32_t vivian_encode_store_instruction(int32_t immediate, uint32_t source_register2,
                                                       uint32_t source_register1, uint32_t function3,
                                                       uint32_t opcode) {
    uint32_t immediate_bits = (uint32_t)immediate;
    return ((immediate_bits & 0xFE0u) << 20) | (source_register2 << 20) | (source_register1 << 15) |
           (function3 << 12) | ((immediate_bits & 0x1Fu) << 7) | opcode;
}
static inline uint32_t vivian_encode_branch_instruction(int32_t immediate, uint32_t source_register2,
                                                        uint32_t source_register1, uint32_t function3,
                                                        uint32_t opcode) {
    uint32_t immediate_bits = (uint32_t)immediate;
    return ((immediate_bits & 0x1000u) << 19) | ((immediate_bits & 0x7E0u) << 20) | (source_register2 << 20) |
           (source_register1 << 15) | (function3 << 12) | ((immediate_bits & 0x1Eu) << 7) |
           ((immediate_bits & 0x800u) >> 4) | opcode;
}
static inline uint32_t vivian_encode_upper_instruction(int32_t immediate, uint32_t destination_register,
                                                       uint32_t opcode) {
    return ((uint32_t)immediate & 0xFFFFF000u) | (destination_register << 7) | opcode;
}
static inline uint32_t vivian_encode_jump_instruction(int32_t immediate, uint32_t destination_register,
                                                      uint32_t opcode) {
    uint32_t immediate_bits = (uint32_t)immediate;
    return ((immediate_bits & 0x100000u) << 11) | ((immediate_bits & 0x7FEu) << 20) |
           ((immediate_bits & 0x800u) << 9) | (immediate_bits & 0xFF000u) | (destination_register << 7) |
           opcode;
}

static inline int vivian_expand_compressed_instruction(uint16_t instruction, uint32_t* expanded_instruction) {
    const uint32_t quadrant = vivian_compressed_bits(instruction, 1, 0);
    const uint32_t function3 = vivian_compressed_bits(instruction, 15, 13);
    const uint32_t destination_register =
        vivian_compressed_bits(instruction, 11, 7); /* also source_register1 in the CI formats */
    const uint32_t source_register2 = vivian_compressed_bits(instruction, 6, 2);

    if (instruction == 0) {
        return 0;
    } /* all-zero is defined as illegal */

    switch (quadrant) {
    case 0: {
        const uint32_t compact_data_register = vivian_compressed_register(instruction, 2);
        const uint32_t compact_source_register1 = vivian_compressed_register(instruction, 7);
        switch (function3) {
        case 0: { /* C.ADDI4SPN */
            uint32_t immediate_bits = (vivian_compressed_bits(instruction, 10, 7) << 6) |
                                      (vivian_compressed_bits(instruction, 12, 11) << 4) |
                                      (vivian_compressed_bits(instruction, 5, 5) << 3) |
                                      (vivian_compressed_bits(instruction, 6, 6) << 2);
            if (immediate_bits == 0) {
                return 0;
            } /* nzuimm==0 is reserved */
            *expanded_instruction = vivian_encode_immediate_instruction(
                (int32_t)immediate_bits, 2, 0, compact_data_register, VIVIAN_OPCODE_IMMEDIATE);
            return 1;
        }
        case 1: { /* C.FLD */
            uint32_t immediate_bits = (vivian_compressed_bits(instruction, 6, 5) << 6) |
                                      (vivian_compressed_bits(instruction, 12, 10) << 3);
            *expanded_instruction =
                vivian_encode_immediate_instruction((int32_t)immediate_bits, compact_source_register1, 3,
                                                    compact_data_register, VIVIAN_OPCODE_FLOAT_LOAD);
            return 1;
        }
        case 2: { /* C.LW */
            uint32_t immediate_bits = (vivian_compressed_bits(instruction, 5, 5) << 6) |
                                      (vivian_compressed_bits(instruction, 12, 10) << 3) |
                                      (vivian_compressed_bits(instruction, 6, 6) << 2);
            *expanded_instruction =
                vivian_encode_immediate_instruction((int32_t)immediate_bits, compact_source_register1, 2,
                                                    compact_data_register, VIVIAN_OPCODE_LOAD);
            return 1;
        }
        case 3: { /* C.FLW */
            uint32_t immediate_bits = (vivian_compressed_bits(instruction, 5, 5) << 6) |
                                      (vivian_compressed_bits(instruction, 12, 10) << 3) |
                                      (vivian_compressed_bits(instruction, 6, 6) << 2);
            *expanded_instruction =
                vivian_encode_immediate_instruction((int32_t)immediate_bits, compact_source_register1, 2,
                                                    compact_data_register, VIVIAN_OPCODE_FLOAT_LOAD);
            return 1;
        }
        case 5: { /* C.FSD */
            uint32_t immediate_bits = (vivian_compressed_bits(instruction, 6, 5) << 6) |
                                      (vivian_compressed_bits(instruction, 12, 10) << 3);
            *expanded_instruction =
                vivian_encode_store_instruction((int32_t)immediate_bits, compact_data_register,
                                                compact_source_register1, 3, VIVIAN_OPCODE_FLOAT_STORE);
            return 1;
        }
        case 6: { /* C.SW */
            uint32_t immediate_bits = (vivian_compressed_bits(instruction, 5, 5) << 6) |
                                      (vivian_compressed_bits(instruction, 12, 10) << 3) |
                                      (vivian_compressed_bits(instruction, 6, 6) << 2);
            *expanded_instruction =
                vivian_encode_store_instruction((int32_t)immediate_bits, compact_data_register,
                                                compact_source_register1, 2, VIVIAN_OPCODE_STORE);
            return 1;
        }
        case 7: { /* C.FSW */
            uint32_t immediate_bits = (vivian_compressed_bits(instruction, 5, 5) << 6) |
                                      (vivian_compressed_bits(instruction, 12, 10) << 3) |
                                      (vivian_compressed_bits(instruction, 6, 6) << 2);
            *expanded_instruction =
                vivian_encode_store_instruction((int32_t)immediate_bits, compact_data_register,
                                                compact_source_register1, 2, VIVIAN_OPCODE_FLOAT_STORE);
            return 1;
        }
        default:
            return 0; /* function3 == 4 is reserved */
        }
    }

    case 1:
        switch (function3) {
        case 0: { /* C.NOP / C.ADDI */
            int32_t immediate = vivian_compressed_sign_extend(
                (vivian_compressed_bits(instruction, 12, 12) << 5) | source_register2, 6);
            *expanded_instruction = vivian_encode_immediate_instruction(
                immediate, destination_register, 0, destination_register,
                VIVIAN_OPCODE_IMMEDIATE); /* destination_register==0 gives a real nop */
            return 1;
        }
        case 1: { /* C.JAL */
            int32_t offset =
                vivian_compressed_sign_extend((vivian_compressed_bits(instruction, 12, 12) << 11) |
                                                  (vivian_compressed_bits(instruction, 8, 8) << 10) |
                                                  (vivian_compressed_bits(instruction, 10, 9) << 8) |
                                                  (vivian_compressed_bits(instruction, 6, 6) << 7) |
                                                  (vivian_compressed_bits(instruction, 7, 7) << 6) |
                                                  (vivian_compressed_bits(instruction, 2, 2) << 5) |
                                                  (vivian_compressed_bits(instruction, 11, 11) << 4) |
                                                  (vivian_compressed_bits(instruction, 5, 3) << 1),
                                              12);
            *expanded_instruction = vivian_encode_jump_instruction(offset, 1, VIVIAN_OPCODE_JUMP);
            return 1;
        }
        case 2: { /* C.LI */
            int32_t immediate = vivian_compressed_sign_extend(
                (vivian_compressed_bits(instruction, 12, 12) << 5) | source_register2, 6);
            if (destination_register == 0) {
                return 0;
            } /* Current policy rejects this hint encoding. */
            *expanded_instruction = vivian_encode_immediate_instruction(immediate, 0, 0, destination_register,
                                                                        VIVIAN_OPCODE_IMMEDIATE);
            return 1;
        }
        case 3: {
            if (destination_register == 2) { /* C.ADDI16SP */
                int32_t immediate =
                    vivian_compressed_sign_extend((vivian_compressed_bits(instruction, 12, 12) << 9) |
                                                      (vivian_compressed_bits(instruction, 4, 3) << 7) |
                                                      (vivian_compressed_bits(instruction, 5, 5) << 6) |
                                                      (vivian_compressed_bits(instruction, 2, 2) << 5) |
                                                      (vivian_compressed_bits(instruction, 6, 6) << 4),
                                                  10);
                if (immediate == 0) {
                    return 0;
                }
                *expanded_instruction =
                    vivian_encode_immediate_instruction(immediate, 2, 0, 2, VIVIAN_OPCODE_IMMEDIATE);
                return 1;
            }
            /* C.LUI */
            int32_t immediate = vivian_compressed_sign_extend(
                (vivian_compressed_bits(instruction, 12, 12) << 5) | source_register2, 6);
            if (destination_register == 0 || immediate == 0) {
                return 0;
            }
            *expanded_instruction = vivian_encode_upper_instruction(
                (int32_t)((uint32_t)immediate << 12), destination_register, VIVIAN_OPCODE_UPPER_IMMEDIATE);
            return 1;
        }
        case 4: {
            const uint32_t compact_data_register = vivian_compressed_register(instruction, 7);
            const uint32_t function2 = vivian_compressed_bits(instruction, 11, 10);
            if (function2 == 0 || function2 == 1) { /* C.SRLI / C.SRAI */
                uint32_t shift_amount = (vivian_compressed_bits(instruction, 12, 12) << 5) | source_register2;
                if (shift_amount >= 32) {
                    return 0;
                } /* shift_amount[5] set is RV64 only */
                *expanded_instruction = vivian_encode_immediate_instruction(
                    (int32_t)((function2 == 1 ? 0x400u : 0u) | shift_amount), compact_data_register, 5,
                    compact_data_register, VIVIAN_OPCODE_IMMEDIATE);
                return 1;
            }
            if (function2 == 2) { /* C.ANDI */
                int32_t immediate = vivian_compressed_sign_extend(
                    (vivian_compressed_bits(instruction, 12, 12) << 5) | source_register2, 6);
                *expanded_instruction = vivian_encode_immediate_instruction(
                    immediate, compact_data_register, 7, compact_data_register, VIVIAN_OPCODE_IMMEDIATE);
                return 1;
            }
            { /* register-register group */
                const uint32_t compact_source_register2 = vivian_compressed_register(instruction, 2);
                const uint32_t operation = vivian_compressed_bits(instruction, 6, 5);
                if (vivian_compressed_bits(instruction, 12, 12) != 0) {
                    return 0;
                } /* C.SUBW/C.ADDW are RV64 */
                switch (operation) {
                case 0:
                    *expanded_instruction = vivian_encode_register_instruction(
                        0x20, compact_source_register2, compact_data_register, 0, compact_data_register,
                        VIVIAN_OPCODE_REGISTER);
                    return 1; /* SUB */
                case 1:
                    *expanded_instruction = vivian_encode_register_instruction(
                        0x00, compact_source_register2, compact_data_register, 4, compact_data_register,
                        VIVIAN_OPCODE_REGISTER);
                    return 1; /* XOR */
                case 2:
                    *expanded_instruction = vivian_encode_register_instruction(
                        0x00, compact_source_register2, compact_data_register, 6, compact_data_register,
                        VIVIAN_OPCODE_REGISTER);
                    return 1; /* OR  */
                default:
                    *expanded_instruction = vivian_encode_register_instruction(
                        0x00, compact_source_register2, compact_data_register, 7, compact_data_register,
                        VIVIAN_OPCODE_REGISTER);
                    return 1; /* AND */
                }
            }
        }
        case 5: { /* C.J */
            int32_t offset =
                vivian_compressed_sign_extend((vivian_compressed_bits(instruction, 12, 12) << 11) |
                                                  (vivian_compressed_bits(instruction, 8, 8) << 10) |
                                                  (vivian_compressed_bits(instruction, 10, 9) << 8) |
                                                  (vivian_compressed_bits(instruction, 6, 6) << 7) |
                                                  (vivian_compressed_bits(instruction, 7, 7) << 6) |
                                                  (vivian_compressed_bits(instruction, 2, 2) << 5) |
                                                  (vivian_compressed_bits(instruction, 11, 11) << 4) |
                                                  (vivian_compressed_bits(instruction, 5, 3) << 1),
                                              12);
            *expanded_instruction = vivian_encode_jump_instruction(offset, 0, VIVIAN_OPCODE_JUMP);
            return 1;
        }
        case 6:   /* C.BEQZ */
        case 7: { /* C.BNEZ */
            const uint32_t compact_source_register1 = vivian_compressed_register(instruction, 7);
            int32_t offset =
                vivian_compressed_sign_extend((vivian_compressed_bits(instruction, 12, 12) << 8) |
                                                  (vivian_compressed_bits(instruction, 6, 5) << 6) |
                                                  (vivian_compressed_bits(instruction, 2, 2) << 5) |
                                                  (vivian_compressed_bits(instruction, 11, 10) << 3) |
                                                  (vivian_compressed_bits(instruction, 4, 3) << 1),
                                              9);
            *expanded_instruction = vivian_encode_branch_instruction(
                offset, 0, compact_source_register1, (function3 == 6) ? 0u : 1u, VIVIAN_OPCODE_BRANCH);
            return 1;
        }
        default:
            return 0;
        }

    case 2:
        switch (function3) {
        case 0: { /* C.SLLI */
            uint32_t shift_amount = (vivian_compressed_bits(instruction, 12, 12) << 5) | source_register2;
            if (destination_register == 0 || shift_amount >= 32) {
                return 0;
            }
            *expanded_instruction =
                vivian_encode_immediate_instruction((int32_t)shift_amount, destination_register, 1,
                                                    destination_register, VIVIAN_OPCODE_IMMEDIATE);
            return 1;
        }
        case 1: { /* C.FLDSP */
            uint32_t immediate_bits = (vivian_compressed_bits(instruction, 4, 2) << 6) |
                                      (vivian_compressed_bits(instruction, 12, 12) << 5) |
                                      (vivian_compressed_bits(instruction, 6, 5) << 3);
            *expanded_instruction = vivian_encode_immediate_instruction(
                (int32_t)immediate_bits, 2, 3, destination_register, VIVIAN_OPCODE_FLOAT_LOAD);
            return 1;
        }
        case 2: { /* C.LWSP */
            uint32_t immediate_bits = (vivian_compressed_bits(instruction, 3, 2) << 6) |
                                      (vivian_compressed_bits(instruction, 12, 12) << 5) |
                                      (vivian_compressed_bits(instruction, 6, 4) << 2);
            if (destination_register == 0) {
                return 0;
            } /* destination_register==0 is reserved */
            *expanded_instruction = vivian_encode_immediate_instruction(
                (int32_t)immediate_bits, 2, 2, destination_register, VIVIAN_OPCODE_LOAD);
            return 1;
        }
        case 3: { /* C.FLWSP */
            uint32_t immediate_bits = (vivian_compressed_bits(instruction, 3, 2) << 6) |
                                      (vivian_compressed_bits(instruction, 12, 12) << 5) |
                                      (vivian_compressed_bits(instruction, 6, 4) << 2);
            *expanded_instruction = vivian_encode_immediate_instruction(
                (int32_t)immediate_bits, 2, 2, destination_register, VIVIAN_OPCODE_FLOAT_LOAD);
            return 1;
        }
        case 4:
            if (vivian_compressed_bits(instruction, 12, 12) == 0) {
                if (source_register2 == 0) { /* C.JR */
                    if (destination_register == 0) {
                        return 0;
                    }
                    *expanded_instruction = vivian_encode_immediate_instruction(0, destination_register, 0, 0,
                                                                                VIVIAN_OPCODE_REGISTER_JUMP);
                    return 1;
                }
                if (destination_register == 0) {
                    return 0;
                } /* hint */
                *expanded_instruction = vivian_encode_register_instruction(
                    0, source_register2, 0, 0, destination_register, VIVIAN_OPCODE_REGISTER); /* C.MV */
                return 1;
            }
            if (source_register2 == 0) {
                if (destination_register == 0) { /* C.EBREAK */
                    *expanded_instruction =
                        vivian_encode_immediate_instruction(1, 0, 0, 0, VIVIAN_OPCODE_SYSTEM);
                    return 1;
                }
                *expanded_instruction = vivian_encode_immediate_instruction(
                    0, destination_register, 0, 1, VIVIAN_OPCODE_REGISTER_JUMP); /* C.JALR */
                return 1;
            }
            if (destination_register == 0) {
                return 0;
            } /* hint */
            *expanded_instruction =
                vivian_encode_register_instruction(0, source_register2, destination_register, 0,
                                                   destination_register, VIVIAN_OPCODE_REGISTER); /* C.ADD */
            return 1;
        case 5: { /* C.FSDSP */
            uint32_t immediate_bits = (vivian_compressed_bits(instruction, 9, 7) << 6) |
                                      (vivian_compressed_bits(instruction, 12, 10) << 3);
            *expanded_instruction = vivian_encode_store_instruction((int32_t)immediate_bits, source_register2,
                                                                    2, 3, VIVIAN_OPCODE_FLOAT_STORE);
            return 1;
        }
        case 6: { /* C.SWSP */
            uint32_t immediate_bits = (vivian_compressed_bits(instruction, 8, 7) << 6) |
                                      (vivian_compressed_bits(instruction, 12, 9) << 2);
            *expanded_instruction = vivian_encode_store_instruction((int32_t)immediate_bits, source_register2,
                                                                    2, 2, VIVIAN_OPCODE_STORE);
            return 1;
        }
        default: { /* C.FSWSP */
            uint32_t immediate_bits = (vivian_compressed_bits(instruction, 8, 7) << 6) |
                                      (vivian_compressed_bits(instruction, 12, 9) << 2);
            *expanded_instruction = vivian_encode_store_instruction((int32_t)immediate_bits, source_register2,
                                                                    2, 2, VIVIAN_OPCODE_FLOAT_STORE);
            return 1;
        }
        }

    default:
        return 0; /* quadrant == 3 is not compressed */
    }
}

#endif
