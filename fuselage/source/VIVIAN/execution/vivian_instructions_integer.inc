/* Private switch fragment. SEE: (VIVIAN.md, Execution loop and instruction families) */

    case 0x13: /* Immediate integer operations */
        /* Unsigned add/subtract preserves RV32 wraparound. */
        switch (function3) {

        case 0x0:
            vivian_write_register(state, destination_register,
                                  (uint32_t)signed_operand1 + (uint32_t)immediate_i);
            break;
        case 0x2:
            vivian_write_register(state, destination_register, (signed_operand1 < immediate_i) ? 1 : 0);
            break;
        case 0x3:
            vivian_write_register(state, destination_register,
                                  ((uint32_t)signed_operand1 < (uint32_t)immediate_i) ? 1 : 0);
            break;
        case 0x4:
            vivian_write_register(state, destination_register, (uint32_t)(signed_operand1 ^ immediate_i));
            break;
        case 0x6:
            vivian_write_register(state, destination_register, (uint32_t)(signed_operand1 | immediate_i));
            break;
        case 0x7:
            vivian_write_register(state, destination_register, (uint32_t)(signed_operand1 & immediate_i));
            break;
        case 0x1:
            if (function7 != 0x00) {
                VIVIAN_FAULT(VIVIAN_ILLEGAL_INSTRUCTION);
            }
            vivian_write_register(state, destination_register,
                                  (uint32_t)signed_operand1 << (source_register2 & 0x1F));
            break;
        case 0x5:
            if (function7 == 0x00) {
                vivian_write_register(state, destination_register,
                                      (uint32_t)signed_operand1 >> (source_register2 & 0x1F));
            } else if (function7 == 0x20) {

                vivian_write_register(state, destination_register,
                                      (uint32_t)(signed_operand1 >> (source_register2 & 0x1F)));
            } else {
                VIVIAN_FAULT(VIVIAN_ILLEGAL_INSTRUCTION);
            }
            break;
        default:
            VIVIAN_FAULT(VIVIAN_ILLEGAL_INSTRUCTION);
        }
        break;

    case 0x33: /* Integer and multiply/divide operations */
        if (function7 == 0x01) {
            uint32_t unsigned_operand1 = (uint32_t)signed_operand1,
                     unsigned_operand2 = (uint32_t)signed_operand2;
            switch (function3) {
            case 0x0:
                vivian_write_register(state, destination_register,
                                      (uint32_t)(unsigned_operand1 * unsigned_operand2));
                break;
            case 0x1:
                vivian_write_register(
                    state, destination_register,
                    (uint32_t)(((int64_t)signed_operand1 * (int64_t)signed_operand2) >> 32));
                break;
            case 0x2:
                vivian_write_register(
                    state, destination_register,
                    (uint32_t)(((int64_t)signed_operand1 * (int64_t)(uint64_t)unsigned_operand2) >> 32));
                break;
            case 0x3:
                vivian_write_register(
                    state, destination_register,
                    (uint32_t)(((uint64_t)unsigned_operand1 * (uint64_t)unsigned_operand2) >> 32));
                break;
            case 0x4:
                if (unsigned_operand2 == 0) {
                    vivian_write_register(state, destination_register, 0xFFFFFFFFu);
                } else if (signed_operand1 == INT32_MIN && signed_operand2 == -1) {
                    vivian_write_register(state, destination_register, (uint32_t)INT32_MIN);
                } else {
                    vivian_write_register(state, destination_register,
                                          (uint32_t)(signed_operand1 / signed_operand2));
                }
                break;
            case 0x5:
                vivian_write_register(state, destination_register,
                                      (unsigned_operand2 == 0) ? 0xFFFFFFFFu
                                                               : (unsigned_operand1 / unsigned_operand2));
                break;
            case 0x6:
                if (unsigned_operand2 == 0) {
                    vivian_write_register(state, destination_register, (uint32_t)signed_operand1);
                } else if (signed_operand1 == INT32_MIN && signed_operand2 == -1) {
                    vivian_write_register(state, destination_register, 0);
                } else {
                    vivian_write_register(state, destination_register,
                                          (uint32_t)(signed_operand1 % signed_operand2));
                }
                break;
            case 0x7:
                vivian_write_register(state, destination_register,
                                      (unsigned_operand2 == 0) ? (uint32_t)signed_operand1
                                                               : (unsigned_operand1 % unsigned_operand2));
                break;
            }
            break;
        }
        switch (function3) {
        case 0x0:

            if (function7 == 0x00) {
                vivian_write_register(state, destination_register,
                                      (uint32_t)signed_operand1 + (uint32_t)signed_operand2);
            } else if (function7 == 0x20) {
                vivian_write_register(state, destination_register,
                                      (uint32_t)signed_operand1 - (uint32_t)signed_operand2);
            } else {
                VIVIAN_FAULT(VIVIAN_ILLEGAL_INSTRUCTION);
            }
            break;
        case 0x1:
            if (function7 != 0x00) {
                VIVIAN_FAULT(VIVIAN_ILLEGAL_INSTRUCTION);
            }
            vivian_write_register(state, destination_register,
                                  (uint32_t)signed_operand1 << (signed_operand2 & 0x1F));
            break;
        case 0x2:
            if (function7 != 0x00) {
                VIVIAN_FAULT(VIVIAN_ILLEGAL_INSTRUCTION);
            }
            vivian_write_register(state, destination_register, (signed_operand1 < signed_operand2) ? 1 : 0);
            break;
        case 0x3:
            if (function7 != 0x00) {
                VIVIAN_FAULT(VIVIAN_ILLEGAL_INSTRUCTION);
            }
            vivian_write_register(state, destination_register,
                                  ((uint32_t)signed_operand1 < (uint32_t)signed_operand2) ? 1 : 0);
            break;
        case 0x4:
            if (function7 != 0x00) {
                VIVIAN_FAULT(VIVIAN_ILLEGAL_INSTRUCTION);
            }
            vivian_write_register(state, destination_register, (uint32_t)(signed_operand1 ^ signed_operand2));
            break;
        case 0x5:
            if (function7 == 0x00) {
                vivian_write_register(state, destination_register,
                                      (uint32_t)signed_operand1 >> (signed_operand2 & 0x1F));
            } else if (function7 == 0x20) {
                vivian_write_register(state, destination_register,
                                      (uint32_t)(signed_operand1 >> (signed_operand2 & 0x1F)));
            } else {
                VIVIAN_FAULT(VIVIAN_ILLEGAL_INSTRUCTION);
            }
            break;
        case 0x6:
            if (function7 != 0x00) {
                VIVIAN_FAULT(VIVIAN_ILLEGAL_INSTRUCTION);
            }
            vivian_write_register(state, destination_register, (uint32_t)(signed_operand1 | signed_operand2));
            break;
        case 0x7:
            if (function7 != 0x00) {
                VIVIAN_FAULT(VIVIAN_ILLEGAL_INSTRUCTION);
            }
            vivian_write_register(state, destination_register, (uint32_t)(signed_operand1 & signed_operand2));
            break;
        default:
            VIVIAN_FAULT(VIVIAN_ILLEGAL_INSTRUCTION);
        }
        break;
