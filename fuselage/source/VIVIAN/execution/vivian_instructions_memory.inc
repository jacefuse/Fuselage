/* Private switch fragment. SEE: (VIVIAN.md, Execution loop and instruction families) */

    case 0x03: /* Integer loads */
    {
        uint32_t address = (uint32_t)signed_operand1 + (uint32_t)immediate_i;
        switch (function3) {
        case 0x0: {
            uint8_t loaded_value;
            if (!vivian_load_byte(state, address, &loaded_value)) {
                VIVIAN_FAULT(VIVIAN_BUS_ERROR);
            }
            vivian_write_register(state, destination_register, (uint32_t)(int32_t)(int8_t)loaded_value);
            break;
        }
        case 0x1: {
            uint16_t loaded_value;
            if (!vivian_load_halfword(state, address, &loaded_value)) {
                VIVIAN_FAULT(VIVIAN_BUS_ERROR);
            }
            vivian_write_register(state, destination_register, (uint32_t)(int32_t)(int16_t)loaded_value);
            break;
        }
        case 0x2: {
            uint32_t loaded_value;
            if (!vivian_load_word(state, address, &loaded_value)) {
                VIVIAN_FAULT(VIVIAN_BUS_ERROR);
            }
            vivian_write_register(state, destination_register, loaded_value);
            break;
        }
        case 0x4: {
            uint8_t loaded_value;
            if (!vivian_load_byte(state, address, &loaded_value)) {
                VIVIAN_FAULT(VIVIAN_BUS_ERROR);
            }
            vivian_write_register(state, destination_register, (uint32_t)loaded_value);
            break;
        }
        case 0x5: {
            uint16_t loaded_value;
            if (!vivian_load_halfword(state, address, &loaded_value)) {
                VIVIAN_FAULT(VIVIAN_BUS_ERROR);
            }
            vivian_write_register(state, destination_register, (uint32_t)loaded_value);
            break;
        }
        default:
            VIVIAN_FAULT(VIVIAN_ILLEGAL_INSTRUCTION);
        }
    } break;

    case 0x23: /* Integer stores */
    {
        uint32_t address = (uint32_t)signed_operand1 + (uint32_t)immediate_s;
        uint32_t byte_count;
        switch (function3) {
        case 0x0:
            if (!vivian_store_byte(state, address, (uint8_t)signed_operand2)) {
                VIVIAN_FAULT(VIVIAN_BUS_ERROR);
            }
            byte_count = 1;
            break;
        case 0x1:
            if (!vivian_store_halfword(state, address, (uint16_t)signed_operand2)) {
                VIVIAN_FAULT(VIVIAN_BUS_ERROR);
            }
            byte_count = 2;
            break;
        case 0x2:
            if (!vivian_store_word(state, address, (uint32_t)signed_operand2)) {
                VIVIAN_FAULT(VIVIAN_BUS_ERROR);
            }
            byte_count = 4;
            break;
        default:
            VIVIAN_FAULT(VIVIAN_ILLEGAL_INSTRUCTION);
        }
        vivian_notify_store(state, address, byte_count);

        if (state->lrValid && address < state->lrAddr + 4 && address + byte_count > state->lrAddr) {
            state->lrValid = 0;
        }
    } break;
