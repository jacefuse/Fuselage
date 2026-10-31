/* Private switch fragment. SEE: (VIVIAN.md, Execution loop and instruction families) */

    case 0x2F: /* Word atomics */
    {

        if (function3 != 0x2) {
            VIVIAN_FAULT(VIVIAN_ILLEGAL_INSTRUCTION);
        }

        const uint32_t operation = function7 >> 2;
        const uint32_t address = (uint32_t)signed_operand1;

        if (address & 3u) {
            VIVIAN_FAULT(VIVIAN_BUS_ERROR);
        }

        if (operation == 0x02) {
            /* LR.W */
            uint32_t loaded_value;
            if (!vivian_load_word(state, address, &loaded_value)) {
                VIVIAN_FAULT(VIVIAN_BUS_ERROR);
            }
            vivian_write_register(state, destination_register, loaded_value);
            state->lrValid = 1;
            state->lrAddr = address;
            break;
        }

        if (operation == 0x03) {
            /* SC.W: zero reports a completed store; nonzero reports no store. */

            if (state->lrValid && state->lrAddr == address) {
                if (!vivian_store_word(state, address, (uint32_t)signed_operand2)) {
                    VIVIAN_FAULT(VIVIAN_BUS_ERROR);
                }
                vivian_notify_store(state, address, 4);
                vivian_write_register(state, destination_register, 0);
            } else {
                vivian_write_register(state, destination_register, 1);
            }
            state->lrValid = 0;
            break;
        }

        {
            uint32_t previous_value;
            uint32_t operand_value = (uint32_t)signed_operand2;
            uint32_t updated_value;
            if (!vivian_load_word(state, address, &previous_value)) {
                VIVIAN_FAULT(VIVIAN_BUS_ERROR);
            }
            switch (operation) {
            case 0x00:
                updated_value = previous_value + operand_value;
                break;
            case 0x01:
                updated_value = operand_value;
                break;
            case 0x04:
                updated_value = previous_value ^ operand_value;
                break;
            case 0x08:
                updated_value = previous_value | operand_value;
                break;
            case 0x0C:
                updated_value = previous_value & operand_value;
                break;
            case 0x10:
                updated_value =
                    ((int32_t)previous_value < (int32_t)operand_value) ? previous_value : operand_value;
                break;
            case 0x14:
                updated_value =
                    ((int32_t)previous_value > (int32_t)operand_value) ? previous_value : operand_value;
                break;
            case 0x18:
                updated_value = (previous_value < operand_value) ? previous_value : operand_value;
                break;
            case 0x1C:
                updated_value = (previous_value > operand_value) ? previous_value : operand_value;
                break;
            default:
                VIVIAN_FAULT(VIVIAN_ILLEGAL_INSTRUCTION);
            }
            if (!vivian_store_word(state, address, updated_value)) {
                VIVIAN_FAULT(VIVIAN_BUS_ERROR);
            }
            vivian_notify_store(state, address, 4);
            if (state->lrValid && state->lrAddr == address) {
                state->lrValid = 0;
            }
            vivian_write_register(state, destination_register, previous_value);
        }
    } break;
