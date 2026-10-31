/* Private switch fragment. SEE: (VIVIAN.md, Execution loop and instruction families) */

    case 0x37: /* LUI */
        vivian_write_register(state, destination_register, (uint32_t)immediate_u);
        break;

    case 0x17: /* AUIPC */
        vivian_write_register(state, destination_register, state->pc + (uint32_t)immediate_u);
        break;

    case 0x6F: /* JAL */
        vivian_write_register(state, destination_register, next_pc);
        next_pc = state->pc + (uint32_t)immediate_j;
        break;

    case 0x67: /* JALR */
        if (function3 != 0) {
            VIVIAN_FAULT(VIVIAN_ILLEGAL_INSTRUCTION);
        }
        {
            uint32_t target_address = ((uint32_t)signed_operand1 + (uint32_t)immediate_i) & ~1u;
            vivian_write_register(state, destination_register, next_pc);
            next_pc = target_address;
        }
        break;

    case 0x63: /* Branches */
    {
        int branch_taken;
        switch (function3) {
        case 0x0:
            branch_taken = (signed_operand1 == signed_operand2);
            break;
        case 0x1:
            branch_taken = (signed_operand1 != signed_operand2);
            break;
        case 0x4:
            branch_taken = (signed_operand1 < signed_operand2);
            break;
        case 0x5:
            branch_taken = (signed_operand1 >= signed_operand2);
            break;
        case 0x6:
            branch_taken = ((uint32_t)signed_operand1 < (uint32_t)signed_operand2);
            break;
        case 0x7:
            branch_taken = ((uint32_t)signed_operand1 >= (uint32_t)signed_operand2);
            break;
        default:
            VIVIAN_FAULT(VIVIAN_ILLEGAL_INSTRUCTION);
        }
        if (branch_taken) {
            next_pc = state->pc + (uint32_t)immediate_b;
        }
    } break;
