/* Private switch fragment. SEE: (VIVIAN.md, Execution loop and instruction families) */

    case 0x07: /* Floating loads */
        if (function3 == 0x2) {
            uint32_t address = (uint32_t)signed_operand1 + (uint32_t)immediate_i, loaded_bits;
            if (!vivian_load_word(state, address, &loaded_bits)) {
                VIVIAN_FAULT(VIVIAN_BUS_ERROR);
            }
            vivian_write_single_bits(state, destination_register, loaded_bits);
        } else if (function3 == 0x3) {
            uint32_t address = (uint32_t)signed_operand1 + (uint32_t)immediate_i;
            uint64_t loaded_bits;
            if (!vivian_load_doubleword(state, address, &loaded_bits)) {
                VIVIAN_FAULT(VIVIAN_BUS_ERROR);
            }
            vivian_write_double_bits(state, destination_register, loaded_bits);
        } else {
            VIVIAN_FAULT(VIVIAN_ILLEGAL_INSTRUCTION);
        }
        break;

    case 0x27: /* Floating stores */
        if (function3 == 0x2) {
            uint32_t address = (uint32_t)signed_operand1 + (uint32_t)immediate_s;
            if (!vivian_store_word(state, address, vivian_read_single_raw_bits(state, source_register2))) {
                VIVIAN_FAULT(VIVIAN_BUS_ERROR);
            }
            vivian_notify_store(state, address, 4);
        } else if (function3 == 0x3) {
            uint32_t address = (uint32_t)signed_operand1 + (uint32_t)immediate_s;
            if (!vivian_store_doubleword(state, address, vivian_read_double_bits(state, source_register2))) {
                VIVIAN_FAULT(VIVIAN_BUS_ERROR);
            }
            vivian_notify_store(state, address, 8);
        } else {
            VIVIAN_FAULT(VIVIAN_ILLEGAL_INSTRUCTION);
        }
        break;
