#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <ctype.h>
#include <math.h>

// Estrutura para mapear registradores para seus números
typedef struct {
    char *name;
    uint8_t number;
} Register;

// Tabela de registradores RISC-V
Register registers[] = {
    {"x0", 0}, {"zero", 0},
    {"x1", 1}, {"ra", 1},
    {"x2", 2}, {"sp", 2},
    {"x3", 3}, {"gp", 3},
    {"x4", 4}, {"tp", 4},
    {"x5", 5}, {"t0", 5},
    {"x6", 6}, {"t1", 6},
    {"x7", 7}, {"t2", 7},
    {"x8", 8}, {"s0", 8}, {"fp", 8},
    {"x9", 9}, {"s1", 9},
    {"x10", 10}, {"a0", 10},
    {"x11", 11}, {"a1", 11},
    {"x12", 12}, {"a2", 12},
    {"x13", 13}, {"a3", 13},
    {"x14", 14}, {"a4", 14},
    {"x15", 15}, {"a5", 15},
    {"x16", 16}, {"a6", 16},
    {"x17", 17}, {"a7", 17},
    {"x18", 18}, {"s2", 18},
    {"x19", 19}, {"s3", 19},
    {"x20", 20}, {"s4", 20},
    {"x21", 21}, {"s5", 21},
    {"x22", 22}, {"s6", 22},
    {"x23", 23}, {"s7", 23},
    {"x24", 24}, {"s8", 24},
    {"x25", 25}, {"s9", 25},
    {"x26", 26}, {"s10", 26},
    {"x27", 27}, {"s11", 27},
    {"x28", 28}, {"t3", 28},
    {"x29", 29}, {"t4", 29},
    {"x30", 30}, {"t5", 30},
    {"x31", 31}, {"t6", 31},
    {NULL, 0}
};

// Estrutura para labels
typedef struct {
    char *name;
    uint32_t address;
} Label;

Label labels[100];
int label_count = 0;

// Função para obter o número do registrador
int get_register_number(char *reg) {
    for (int i = 0; registers[i].name != NULL; i++) {
        if (strcmp(registers[i].name, reg) == 0) {
            return registers[i].number;
        }
    }
    return -1; // Registrador não encontrado
}

// Função para converter string para inteiro, lidando com hexadecimal, binário e decimal
int parse_immediate(char *str) {
    if (str == NULL) return 0;
    
    // Verifica se é hexadecimal (começa com 0x ou 0X)
    if (strncmp(str, "0x", 2) == 0 || strncmp(str, "0X", 2) == 0) {
        return (int)strtol(str, NULL, 16);
    }
    
    // Verifica se é binário (começa com 0b ou 0B)
    if (strncmp(str, "0b", 2) == 0 || strncmp(str, "0B", 2) == 0) {
        return (int)strtol(str + 2, NULL, 2);
    }
    
    // Verifica se é octal (começa com 0)
    if (str[0] == '0' && strlen(str) > 1) {
        return (int)strtol(str, NULL, 8);
    }
    
    // Caso contrário, assume decimal
    return atoi(str);
}

// Funções para montar diferentes tipos de instruções
uint32_t assemble_r_type(uint8_t opcode, uint8_t funct3, uint8_t funct7, uint8_t rd, uint8_t rs1, uint8_t rs2) {
    return (funct7 << 25) | (rs2 << 20) | (rs1 << 15) | (funct3 << 12) | (rd << 7) | opcode;
}

uint32_t assemble_i_type(uint8_t opcode, uint8_t funct3, uint8_t rd, uint8_t rs1, int imm) {
    imm = imm & 0xFFF;
    if (imm & 0x800) {
        imm |= 0xFFFFF000;
    }
    return (imm << 20) | (rs1 << 15) | (funct3 << 12) | (rd << 7) | opcode;
}

uint32_t assemble_s_type(uint8_t opcode, uint8_t funct3, uint8_t rs1, uint8_t rs2, int imm) {
    uint32_t imm_lo = imm & 0x1F;
    uint32_t imm_hi = (imm >> 5) & 0x7F;
    return (imm_hi << 25) | (rs2 << 20) | (rs1 << 15) | (funct3 << 12) | (imm_lo << 7) | opcode;
}

uint32_t assemble_b_type(uint8_t opcode, uint8_t funct3, uint8_t rs1, uint8_t rs2, int imm) {
    uint32_t imm_11 = (imm >> 11) & 0x1;
    uint32_t imm_4_1 = (imm >> 1) & 0xF;
    uint32_t imm_10_5 = (imm >> 5) & 0x3F;
    uint32_t imm_12 = (imm >> 12) & 0x1;
    return (imm_12 << 31) | (imm_10_5 << 25) | (rs2 << 20) | (rs1 << 15) | (funct3 << 12) | (imm_4_1 << 8) | (imm_11 << 7) | opcode;
}

uint32_t assemble_u_type(uint8_t opcode, uint8_t rd, int imm) {
    return (imm & 0xFFFFF000) | (rd << 7) | opcode;
}

uint32_t assemble_j_type(uint8_t opcode, uint8_t rd, int imm) {
    uint32_t imm_20 = (imm >> 20) & 0x1;
    uint32_t imm_10_1 = (imm >> 1) & 0x3FF;
    uint32_t imm_11 = (imm >> 11) & 0x1;
    uint32_t imm_19_12 = (imm >> 12) & 0xFF;
    return (imm_20 << 31) | (imm_10_1 << 21) | (imm_11 << 20) | (imm_19_12 << 12) | (rd << 7) | opcode;
}

// Função para processar pseudo-instruções
int process_pseudo_instruction(char *op, char *rd, char *rs1, char *rs2, char *imm, FILE *output) {
    uint32_t inst1 = 0, inst2 = 0;
    
    if (strcmp(op, "nop") == 0) {
        inst1 = assemble_i_type(0x13, 0x0, 0, 0, 0);
    } 
    else if (strcmp(op, "li") == 0) {
        int imm_val = parse_immediate(imm);
        int rd_num = get_register_number(rd);
        
        // Para valores pequenos, usa addi
        if (imm_val >= -2048 && imm_val < 2048) {
            inst1 = assemble_i_type(0x13, 0x0, rd_num, 0, imm_val);
        } 
        // Para valores maiores, usa lui + addi
        else {
            int upper = (imm_val + 0x800) >> 12;
            int lower = imm_val & 0xFFF;
            inst1 = assemble_u_type(0x37, rd_num, upper << 12);
            inst2 = assemble_i_type(0x13, 0x0, rd_num, rd_num, lower);
        }
    } 
    else if (strcmp(op, "mv") == 0) {
        int rd_num = get_register_number(rd);
        int rs1_num = get_register_number(rs1);
        inst1 = assemble_i_type(0x13, 0x0, rd_num, rs1_num, 0);
    } 
    else if (strcmp(op, "not") == 0) {
        int rd_num = get_register_number(rd);
        int rs1_num = get_register_number(rs1);
        inst1 = assemble_i_type(0x13, 0x6, rd_num, rs1_num, -1);
    } 
    else if (strcmp(op, "neg") == 0) {
        int rd_num = get_register_number(rd);
        int rs1_num = get_register_number(rs1);
        inst1 = assemble_r_type(0x33, 0x0, 0x20, rd_num, 0, rs1_num);
    } 
    else if (strcmp(op, "j") == 0) {
        // Pseudo-instrução para jal x0, offset
        int imm_val = parse_immediate(imm);
        inst1 = assemble_j_type(0x6F, 0, imm_val);
    } 
    else if (strcmp(op, "ret") == 0) {
        // Pseudo-instrução para jalr x0, 0(x1)
        inst1 = assemble_i_type(0x67, 0x0, 0, 1, 0);
    } 
    else if (strcmp(op, "call") == 0) {
        // Pseudo-instrução para auipc + jalr
        // Implementação simplificada - na prática precisaria calcular offsets
        fprintf(stderr, "A pseudo-instrução 'call' não está totalmente implementada\n");
        return -1;
    } 
    else {
        return 0; // Não é uma pseudo-instrução reconhecida
    }
    
    // Escreve as instruções geradas
    if (output) {
        fprintf(output, "%08x\n", inst1);
        if (inst2 != 0) fprintf(output, "%08x\n", inst2);
    } else {
        printf("%08x\n", inst1);
        if (inst2 != 0) printf("%08x\n", inst2);
    }
    
    return 1; // Pseudo-instrução processada
}

// Função para montar instrução específica
uint32_t assemble_instruction(char *op, char *rd, char *rs1, char *rs2, char *imm) {
    int rd_num = get_register_number(rd);
    int rs1_num = get_register_number(rs1);
    int rs2_num = rs2 ? get_register_number(rs2) : 0;
    int imm_val = imm ? parse_immediate(imm) : 0;
    
    // Instruções base
    if (strcmp(op, "add") == 0) return assemble_r_type(0x33, 0x0, 0x00, rd_num, rs1_num, rs2_num);
    if (strcmp(op, "sub") == 0) return assemble_r_type(0x33, 0x0, 0x20, rd_num, rs1_num, rs2_num);
    if (strcmp(op, "and") == 0) return assemble_r_type(0x33, 0x7, 0x00, rd_num, rs1_num, rs2_num);
    if (strcmp(op, "or") == 0) return assemble_r_type(0x33, 0x6, 0x00, rd_num, rs1_num, rs2_num);
    if (strcmp(op, "xor") == 0) return assemble_r_type(0x33, 0x4, 0x00, rd_num, rs1_num, rs2_num);
    if (strcmp(op, "sll") == 0) return assemble_r_type(0x33, 0x1, 0x00, rd_num, rs1_num, rs2_num);
    if (strcmp(op, "srl") == 0) return assemble_r_type(0x33, 0x5, 0x00, rd_num, rs1_num, rs2_num);
    if (strcmp(op, "sra") == 0) return assemble_r_type(0x33, 0x5, 0x20, rd_num, rs1_num, rs2_num);
    
    // Instruções I-type
    if (strcmp(op, "addi") == 0) return assemble_i_type(0x13, 0x0, rd_num, rs1_num, imm_val);
    if (strcmp(op, "andi") == 0) return assemble_i_type(0x13, 0x7, rd_num, rs1_num, imm_val);
    if (strcmp(op, "ori") == 0) return assemble_i_type(0x13, 0x6, rd_num, rs1_num, imm_val);
    if (strcmp(op, "xori") == 0) return assemble_i_type(0x13, 0x4, rd_num, rs1_num, imm_val);
    if (strcmp(op, "slti") == 0) return assemble_i_type(0x13, 0x2, rd_num, rs1_num, imm_val);
    if (strcmp(op, "sltiu") == 0) return assemble_i_type(0x13, 0x3, rd_num, rs1_num, imm_val);
    
    // Load/store
    if (strcmp(op, "lb") == 0) return assemble_i_type(0x03, 0x0, rd_num, rs1_num, imm_val);
    if (strcmp(op, "lh") == 0) return assemble_i_type(0x03, 0x1, rd_num, rs1_num, imm_val);
    if (strcmp(op, "lw") == 0) return assemble_i_type(0x03, 0x2, rd_num, rs1_num, imm_val);
    if (strcmp(op, "lbu") == 0) return assemble_i_type(0x03, 0x4, rd_num, rs1_num, imm_val);
    if (strcmp(op, "lhu") == 0) return assemble_i_type(0x03, 0x5, rd_num, rs1_num, imm_val);
    if (strcmp(op, "sb") == 0) return assemble_s_type(0x23, 0x0, rs1_num, rs2_num, imm_val);
    if (strcmp(op, "sh") == 0) return assemble_s_type(0x23, 0x1, rs1_num, rs2_num, imm_val);
    if (strcmp(op, "sw") == 0) return assemble_s_type(0x23, 0x2, rs1_num, rs2_num, imm_val);
    
    // Branch
    if (strcmp(op, "beq") == 0) return assemble_b_type(0x63, 0x0, rs1_num, rs2_num, imm_val);
    if (strcmp(op, "bne") == 0) return assemble_b_type(0x63, 0x1, rs1_num, rs2_num, imm_val);
    if (strcmp(op, "blt") == 0) return assemble_b_type(0x63, 0x4, rs1_num, rs2_num, imm_val);
    if (strcmp(op, "bge") == 0) return assemble_b_type(0x63, 0x5, rs1_num, rs2_num, imm_val);
    if (strcmp(op, "bltu") == 0) return assemble_b_type(0x63, 0x6, rs1_num, rs2_num, imm_val);
    if (strcmp(op, "bgeu") == 0) return assemble_b_type(0x63, 0x7, rs1_num, rs2_num, imm_val);
    
    // Jump
    if (strcmp(op, "jal") == 0) return assemble_j_type(0x6F, rd_num, imm_val);
    if (strcmp(op, "jalr") == 0) return assemble_i_type(0x67, 0x0, rd_num, rs1_num, imm_val);
    
    // U-type
    if (strcmp(op, "lui") == 0) return assemble_u_type(0x37, rd_num, imm_val);
    if (strcmp(op, "auipc") == 0) return assemble_u_type(0x17, rd_num, imm_val);
    
    return 0; // Instrução não reconhecida
}

// Função para processar uma linha de assembly
int process_line(char *line, FILE *output, uint32_t *address) {
    char op[10], rd[10], rs1[10], rs2[10], imm[20];
    int count;
    
    // Remove comentários (tudo após '#')
    char *comment = strchr(line, '#');
    if (comment) *comment = '\0';
    
    // Remove espaços em branco no início e fim
    while (isspace(*line)) line++;
    char *end = line + strlen(line) - 1;
    while (end > line && isspace(*end)) end--;
    *(end + 1) = '\0';
    
    // Ignora linhas vazias
    if (strlen(line) == 0) return 0;
    
    // Verifica se é uma label (termina com ':')
    if (line[strlen(line)-1] == ':') {
        line[strlen(line)-1] = '\0';
        labels[label_count].name = strdup(line);
        labels[label_count].address = *address;
        label_count++;
        return 0;
    }
    
    // Divide a linha em tokens
    count = sscanf(line, "%s %[^,],%[^,],%s", op, rd, rs1, rs2);
    if (count < 3) {
        count = sscanf(line, "%s %[^,],%s", op, rd, imm);
        if (count < 2) {
            fprintf(stderr, "Erro ao analisar a linha: %s\n", line);
            return -1;
        }
    }
    
    // Remove espaços dos registradores
    char *temp = rs1;
    while (isspace(*temp)) temp++;
    strcpy(rs1, temp);
    
    // Tenta processar como pseudo-instrução primeiro
    int pseudo_result = process_pseudo_instruction(op, rd, rs1, rs2, imm, output);
    if (pseudo_result != 0) {
        *address += (pseudo_result == 1) ? 4 : 8; // Avança 4 ou 8 bytes
        return pseudo_result > 0 ? 0 : -1;
    }
    
    // Monta a instrução normal
    uint32_t instruction;
    if (count == 4) {
        // Verifica se o último argumento é um immediate
        char *ptr;
        strtol(rs2, &ptr, 0);
        if (*ptr == '\0') {
            strcpy(imm, rs2);
            instruction = assemble_instruction(op, rd, rs1, NULL, imm);
        } else {
            instruction = assemble_instruction(op, rd, rs1, rs2, NULL);
        }
    } else if (count == 3) {
        instruction = assemble_instruction(op, rd, rs1, NULL, imm);
    } else {
        instruction = assemble_instruction(op, rd, NULL, NULL, imm);
    }
    
    if (instruction == 0) {
        fprintf(stderr, "Instrução não reconhecida: %s\n", op);
        return -1;
    }
    
    // Escreve a instrução
    if (output) {
        fprintf(output, "%08x\n", instruction);
    } else {
        printf("%08x\n", instruction);
    }
    
    *address += 4; // Avança 4 bytes para próxima instrução
    return 0;
}

int main(int argc, char *argv[]) {
    FILE *input = NULL;
    FILE *output = NULL;
    char *output_filename = NULL;
    uint32_t current_address = 0;
    
    // Processa argumentos de linha de comando
    if (argc < 2) {
        fprintf(stderr, "Uso: %s entrada.asm [-o saida]\n", argv[0]);
        return 1;
    }
    
    // Verifica o parâmetro -o
    for (int i = 2; i < argc; i++) {
        if (strcmp(argv[i], "-o") == 0 && i + 1 < argc) {
            output_filename = argv[i + 1];
            break;
        }
    }
    
    // Abre o arquivo de entrada
    input = fopen(argv[1], "r");
    if (!input) {
        perror("Erro ao abrir arquivo de entrada");
        return 1;
    }
    
    // Abre o arquivo de saída, se especificado
    if (output_filename) {
        output = fopen(output_filename, "w");
        if (!output) {
            perror("Erro ao criar arquivo de saída");
            fclose(input);
            return 1;
        }
    }
    
    // Primeira passagem para coletar labels
    char line[256];
    while (fgets(line, sizeof(line), input)) {
        // Remove comentários e espaços
        char *comment = strchr(line, '#');
        if (comment) *comment = '\0';
        while (isspace(*line)) line++;
        char *end = line + strlen(line) - 1;
        while (end > line && isspace(*end)) end--;
        *(end + 1) = '\0';
        
        // Verifica se é label
        if (strlen(line) > 0 && line[strlen(line)-1] == ':') {
            line[strlen(line)-1] = '\0';
            labels[label_count].name = strdup(line);
            labels[label_count].address = current_address;
            label_count++;
        } 
        // Conta instruções normais
        else if (strlen(line) > 0) {
            current_address += 4;
        }
    }
    
    // Reinicia para segunda passagem
    rewind(input);
    current_address = 0;
    
    // Processa cada linha do arquivo de entrada
    while (fgets(line, sizeof(line), input)) {
        if (process_line(line, output, &current_address) != 0) {
            fclose(input);
            if (output) fclose(output);
            return 1;
   E     }
    }
    
    // Fecha os arquivos
    fclose(input);
    if (output) fclose(output);
    
    return 0;
}
