/* Private switch fragment. SEE: (VIVIAN.md, Execution loop and instruction families) */

    case 0x73: /* ECALL, EBREAK and FP CSRs */
        if (function3 == 0) {

            if (immediate_i == 0x000) {
                VIVIAN_FAULT(VIVIAN_ECALL);
            }
            if (immediate_i == 0x001) {
                VIVIAN_FAULT(VIVIAN_HALT);
            }
            VIVIAN_FAULT(VIVIAN_ILLEGAL_INSTRUCTION);
        }

        {
            uint32_t csr_number = (uint32_t)immediate_i & 0xFFF;

            if (floating_flags_pending) {
                /* Publish pending exceptions before the guest reads or replaces flags. */
                vivian_accrue_float_flags(state);
            }
            uint32_t previous_value;
            switch (csr_number) {
            case 0x001:
                previous_value = state->fcsr & 0x1Fu;
                break;
            case 0x002:
                previous_value = (state->fcsr >> 5) & 0x07u;
                break;
            case 0x003:
                previous_value = state->fcsr & 0xFFu;
                break;
            default:
                VIVIAN_FAULT(VIVIAN_ILLEGAL_INSTRUCTION);
            }

            uint32_t source_value =
                (function3 & 0x4u) ? source_register1 : vivian_read_register(state, source_register1);
            uint32_t updated_value = previous_value;
            switch (function3 & 0x3u) {
            case 1:
                updated_value = source_value;
                break;
            case 2:
                updated_value = previous_value | source_value;
                break;
            case 3:
                updated_value = previous_value & ~source_value;
                break;
            default:
                break;
            }

            {
                uint32_t new_rounding_mode = (csr_number == 0x002)   ? (updated_value & 0x07u)
                                             : (csr_number == 0x003) ? ((updated_value >> 5) & 0x07u)
                                                                     : 0u;
                if (new_rounding_mode != 0u) {
                    VIVIAN_FAULT(VIVIAN_UNSUPPORTED_ROUNDING);
                }
            }
            switch (csr_number) {
            case 0x001:
                state->fcsr = (state->fcsr & ~0x1Fu) | (updated_value & 0x1Fu);
                break;
            case 0x002:
                state->fcsr = (state->fcsr & ~0xE0u) | ((updated_value & 0x07u) << 5);
                break;
            case 0x003:
                state->fcsr = updated_value & 0xFFu;
                break;
            }

            /* Prevent cleared guest flags from being accrued again at slice end. */
            vivian_clear_host_float_flags();
            floating_flags_pending = 0;
            vivian_write_register(state, destination_register, previous_value);
        }
        break;
