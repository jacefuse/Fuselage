/* Private switch fragment. SEE: (VIVIAN.md, Execution loop and instruction families) */

    case 0x43: /* FMADD */
    case 0x47: /* FMSUB */
    case 0x4B: /* FNMSUB */
    case 0x4F: /* FNMADD */ {
        uint32_t format = (instruction >> 25) & 0x3, source_register3 = (instruction >> 27) & 0x1F;
        VIVIAN_ARM_FLOAT_FLAGS();
        if (!vivian_is_nearest_rounding(function3)) {
            VIVIAN_FAULT(VIVIAN_UNSUPPORTED_ROUNDING);
        }
        if (format == 0) {
            float operand1 = vivian_read_single(state, source_register1),
                  operand2 = vivian_read_single(state, source_register2),
                  operand3 = vivian_read_single(state, source_register3), result;
            switch (opcode) {
            case 0x43:
                result = fmaf(operand1, operand2, operand3);
                break;
            case 0x47:
                result = fmaf(operand1, operand2, -operand3);
                break;
            case 0x4B:
                result = fmaf(-operand1, operand2, operand3);
                break;
            default:
                result = fmaf(-operand1, operand2, -operand3);
                break;
            }
            vivian_write_single(state, destination_register, result);
        } else if (format == 1) {
            double operand1 = vivian_read_double(state, source_register1),
                   operand2 = vivian_read_double(state, source_register2),
                   operand3 = vivian_read_double(state, source_register3), result;
            switch (opcode) {
            case 0x43:
                result = fma(operand1, operand2, operand3);
                break;
            case 0x47:
                result = fma(operand1, operand2, -operand3);
                break;
            case 0x4B:
                result = fma(-operand1, operand2, operand3);
                break;
            default:
                result = fma(-operand1, operand2, -operand3);
                break;
            }
            vivian_write_double(state, destination_register, result);
        } else {
            VIVIAN_FAULT(VIVIAN_ILLEGAL_INSTRUCTION);
        }
        break;
    }

    case 0x53: /* Floating operations */ {
        float operand1 = vivian_read_single(state, source_register1),
              operand2 = vivian_read_single(state, source_register2);
        VIVIAN_ARM_FLOAT_FLAGS();

        switch (function7) {
        case 0x00:
        case 0x04:
        case 0x08:
        case 0x0C:
        case 0x2C:
        case 0x01:
        case 0x05:
        case 0x09:
        case 0x0D:
        case 0x2D:
        case 0x20:
        case 0x21:
        case 0x68:
        case 0x69:
            if (!vivian_is_nearest_rounding(function3)) {
                VIVIAN_FAULT(VIVIAN_UNSUPPORTED_ROUNDING);
            }
            break;
        default:
            break;
        }
        switch (function7) {
        case 0x00:
            vivian_write_single(state, destination_register, operand1 + operand2);
            break;
        case 0x04:
            vivian_write_single(state, destination_register, operand1 - operand2);
            break;
        case 0x08:
            vivian_write_single(state, destination_register, operand1 * operand2);
            break;
        case 0x0C:
            vivian_write_single(state, destination_register, operand1 / operand2);
            break;
        case 0x2C:
            vivian_write_single(state, destination_register, sqrtf(operand1));
            break;
        case 0x10: {
            uint32_t operand1_bits = vivian_read_single_bits(state, source_register1),
                     operand2_bits = vivian_read_single_bits(state, source_register2), sign;
            switch (function3) {
            case 0:
                sign = operand2_bits & 0x80000000u;
                break;
            case 1:
                sign = ~operand2_bits & 0x80000000u;
                break;
            case 2:
                sign = (operand1_bits ^ operand2_bits) & 0x80000000u;
                break;
            default:
                VIVIAN_FAULT(VIVIAN_ILLEGAL_INSTRUCTION);
            }
            vivian_write_single_bits(state, destination_register, (operand1_bits & 0x7FFFFFFFu) | sign);
            break;
        }
        case 0x14:
            if (vivian_is_signaling_nan_single(vivian_read_single_bits(state, source_register1)) ||
                vivian_is_signaling_nan_single(vivian_read_single_bits(state, source_register2))) {
                feraiseexcept(FE_INVALID);
            }
            if (function3 == 0) {
                vivian_write_single(state, destination_register, fminf(operand1, operand2));
            } else if (function3 == 1) {
                vivian_write_single(state, destination_register, fmaxf(operand1, operand2));
            } else {
                VIVIAN_FAULT(VIVIAN_ILLEGAL_INSTRUCTION);
            }
            break;
        case 0x50:
            switch (function3) {
            case 0:
                vivian_write_register(state, destination_register, (operand1 <= operand2) ? 1u : 0u);
                break;
            case 1:
                vivian_write_register(state, destination_register, (operand1 < operand2) ? 1u : 0u);
                break;
            case 2:
                vivian_write_register(state, destination_register, (operand1 == operand2) ? 1u : 0u);
                break;
            default:
                VIVIAN_FAULT(VIVIAN_ILLEGAL_INSTRUCTION);
            }
            break;
        case 0x60: {
            int invalid_rounding_mode = 0;
            uint32_t converted_word;
            if (source_register2 == 0) {
                converted_word = vivian_convert_float_to_word(operand1, 0, function3, &invalid_rounding_mode);
                if (invalid_rounding_mode) {
                    VIVIAN_FAULT(VIVIAN_ILLEGAL_INSTRUCTION);
                }
                vivian_write_register(state, destination_register, converted_word);
            } else if (source_register2 == 1) {
                converted_word = vivian_convert_float_to_word(operand1, 1, function3, &invalid_rounding_mode);
                if (invalid_rounding_mode) {
                    VIVIAN_FAULT(VIVIAN_ILLEGAL_INSTRUCTION);
                }
                vivian_write_register(state, destination_register, converted_word);
            } else {
                VIVIAN_FAULT(VIVIAN_ILLEGAL_INSTRUCTION);
            }
            break;
        }
        case 0x68:
            if (source_register2 == 0) {
                vivian_write_single(state, destination_register,
                                    (float)(int32_t)vivian_read_register(state, source_register1));
            } else if (source_register2 == 1) {
                vivian_write_single(state, destination_register,
                                    (float)vivian_read_register(state, source_register1));
            } else {
                VIVIAN_FAULT(VIVIAN_ILLEGAL_INSTRUCTION);
            }
            break;
        case 0x70:
            if (function3 == 0 && source_register2 == 0) {
                vivian_write_register(state, destination_register,
                                      vivian_read_single_raw_bits(state, source_register1));
            } else if (function3 == 1 && source_register2 == 0) {
                vivian_write_register(
                    state, destination_register,
                    vivian_classify_single(vivian_read_single_bits(state, source_register1)));
            } else {
                VIVIAN_FAULT(VIVIAN_ILLEGAL_INSTRUCTION);
            }
            break;
        case 0x78:
            if (function3 == 0 && source_register2 == 0) {
                vivian_write_single_bits(state, destination_register,
                                         vivian_read_register(state, source_register1));
            } else {
                VIVIAN_FAULT(VIVIAN_ILLEGAL_INSTRUCTION);
            }
            break;

        case 0x01:
            vivian_write_double(state, destination_register,
                                vivian_read_double(state, source_register1) +
                                    vivian_read_double(state, source_register2));
            break;
        case 0x05:
            vivian_write_double(state, destination_register,
                                vivian_read_double(state, source_register1) -
                                    vivian_read_double(state, source_register2));
            break;
        case 0x09:
            vivian_write_double(state, destination_register,
                                vivian_read_double(state, source_register1) *
                                    vivian_read_double(state, source_register2));
            break;
        case 0x0D:
            vivian_write_double(state, destination_register,
                                vivian_read_double(state, source_register1) /
                                    vivian_read_double(state, source_register2));
            break;
        case 0x2D:
            vivian_write_double(state, destination_register,
                                sqrt(vivian_read_double(state, source_register1)));
            break;
        case 0x11: {
            uint64_t operand1_bits = vivian_read_double_bits(state, source_register1),
                     operand2_bits = vivian_read_double_bits(state, source_register2), sign;
            switch (function3) {
            case 0:
                sign = operand2_bits & 0x8000000000000000ULL;
                break;
            case 1:
                sign = ~operand2_bits & 0x8000000000000000ULL;
                break;
            case 2:
                sign = (operand1_bits ^ operand2_bits) & 0x8000000000000000ULL;
                break;
            default:
                VIVIAN_FAULT(VIVIAN_ILLEGAL_INSTRUCTION);
            }
            vivian_write_double_bits(state, destination_register,
                                     (operand1_bits & 0x7FFFFFFFFFFFFFFFULL) | sign);
            break;
        }
        case 0x15: {
            double double_operand1 = vivian_read_double(state, source_register1),
                   double_operand2 = vivian_read_double(state, source_register2);
            if (vivian_is_signaling_nan_double(vivian_read_double_bits(state, source_register1)) ||
                vivian_is_signaling_nan_double(vivian_read_double_bits(state, source_register2))) {
                feraiseexcept(FE_INVALID);
            }
            if (function3 == 0) {
                vivian_write_double(state, destination_register, fmin(double_operand1, double_operand2));
            } else if (function3 == 1) {
                vivian_write_double(state, destination_register, fmax(double_operand1, double_operand2));
            } else {
                VIVIAN_FAULT(VIVIAN_ILLEGAL_INSTRUCTION);
            }
            break;
        }
        case 0x51: {
            double double_operand1 = vivian_read_double(state, source_register1),
                   double_operand2 = vivian_read_double(state, source_register2);
            switch (function3) {
            case 0:
                vivian_write_register(state, destination_register,
                                      (double_operand1 <= double_operand2) ? 1u : 0u);
                break;
            case 1:
                vivian_write_register(state, destination_register,
                                      (double_operand1 < double_operand2) ? 1u : 0u);
                break;
            case 2:
                vivian_write_register(state, destination_register,
                                      (double_operand1 == double_operand2) ? 1u : 0u);
                break;
            default:
                VIVIAN_FAULT(VIVIAN_ILLEGAL_INSTRUCTION);
            }
            break;
        }
        case 0x71:
            if (function3 == 1 && source_register2 == 0) {
                vivian_write_register(
                    state, destination_register,
                    vivian_classify_double(vivian_read_double_bits(state, source_register1)));
            } else {
                VIVIAN_FAULT(VIVIAN_ILLEGAL_INSTRUCTION);
            }
            break;
        case 0x61: {
            int invalid_rounding_mode = 0;
            uint32_t converted_word;
            if (source_register2 == 0) {
                converted_word = vivian_convert_float_to_word(vivian_read_double(state, source_register1), 0,
                                                              function3, &invalid_rounding_mode);
                if (invalid_rounding_mode) {
                    VIVIAN_FAULT(VIVIAN_ILLEGAL_INSTRUCTION);
                }
                vivian_write_register(state, destination_register, converted_word);
            } else if (source_register2 == 1) {
                converted_word = vivian_convert_float_to_word(vivian_read_double(state, source_register1), 1,
                                                              function3, &invalid_rounding_mode);
                if (invalid_rounding_mode) {
                    VIVIAN_FAULT(VIVIAN_ILLEGAL_INSTRUCTION);
                }
                vivian_write_register(state, destination_register, converted_word);
            } else {
                VIVIAN_FAULT(VIVIAN_ILLEGAL_INSTRUCTION);
            }
            break;
        }
        case 0x69:
            if (source_register2 == 0) {
                vivian_write_double(state, destination_register,
                                    (double)(int32_t)vivian_read_register(state, source_register1));
            } else if (source_register2 == 1) {
                vivian_write_double(state, destination_register,
                                    (double)vivian_read_register(state, source_register1));
            } else {
                VIVIAN_FAULT(VIVIAN_ILLEGAL_INSTRUCTION);
            }
            break;
        case 0x20:
            if (source_register2 == 1) {
                vivian_write_single(state, destination_register,
                                    (float)vivian_read_double(state, source_register1));
            } else {
                VIVIAN_FAULT(VIVIAN_ILLEGAL_INSTRUCTION);
            }
            break;
        case 0x21:
            if (source_register2 == 0) {
                vivian_write_double(state, destination_register,
                                    (double)vivian_read_single(state, source_register1));
            } else {
                VIVIAN_FAULT(VIVIAN_ILLEGAL_INSTRUCTION);
            }
            break;
        default:
            VIVIAN_FAULT(VIVIAN_ILLEGAL_INSTRUCTION);
        }
        break;
    }
