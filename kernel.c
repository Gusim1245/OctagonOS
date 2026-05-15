#include <stdint.h>

// ─── I/O ────────────────────────────────────────────────────────────────────

static inline uint8_t inb(uint16_t port) {
    uint8_t value;
    __asm__ volatile ("inb %1, %0" : "=a"(value) : "Nd"(port));
    return value;
}

static inline void outb(uint16_t port, uint8_t value) {
    __asm__ volatile ("outb %0, %1" : : "a"(value), "Nd"(port));
}

// ─── IDT ────────────────────────────────────────────────────────────────────

struct idt_entry {
    uint16_t base_low;
    uint16_t selector;
    uint8_t  zero;
    uint8_t  flags;
    uint16_t base_high;
} __attribute__((packed));

struct idt_ptr {
    uint16_t limit;
    uint32_t base;
} __attribute__((packed));

#define IDT_SIZE 256
static struct idt_entry idt[IDT_SIZE];
static struct idt_ptr   idtr;

static void idt_set_gate(uint8_t vector, uint32_t handler) {
    idt[vector].base_low  = handler & 0xFFFF;
    idt[vector].selector  = 0x08;          // segmento de código (GDT)
    idt[vector].zero      = 0;
    idt[vector].flags     = 0x8E;          // presente, DPL=0, 32-bit interrupt gate
    idt[vector].base_high = (handler >> 16) & 0xFFFF;
}

static void idt_install(void) {
    idtr.limit = sizeof(idt) - 1;
    idtr.base  = (uint32_t)&idt;
    __asm__ volatile ("lidt %0" : : "m"(idtr));
}

// ─── PIC ────────────────────────────────────────────────────────────────────
// Remapeia IRQ 0-7 para vetores 0x20-0x27 (evita conflito com exceções do CPU)
// Adicione essa função no kernel.c, logo após o outb/inb
static void io_wait(void) {
    // porta 0x80 é usada para pequeno delay de I/O (~1-4 microsegundos)
    outb(0x80, 0x00);
}

static void pic_remap(void) {
    outb(0x20, 0x11);
    outb(0xA0, 0x11);
    io_wait();              // aguarda o PIC processar
    outb(0x21, 0x20);
    outb(0xA1, 0x28);
    io_wait();
    outb(0x21, 0x04);
    outb(0xA1, 0x02);
    io_wait();
    outb(0x21, 0x01);
    outb(0xA1, 0x01);
    io_wait();
    outb(0x21, 0xFE);      // habilita só IRQ0
    outb(0xA1, 0xFF);      // mascara tudo no escravo
}
// ─── PIT ────────────────────────────────────────────────────────────────────

#define PIT_FREQUENCY 1193182

volatile uint32_t ticks = 0;

static void pit_init(uint32_t frequency) {
    uint32_t divisor = PIT_FREQUENCY / frequency;
    outb(0x43, 0x36);
    outb(0x40, divisor & 0xFF);
    outb(0x40, (divisor >> 8) & 0xFF);
}

// Handler precisa de um stub em assembly para salvar/restaurar registradores
void pit_handler_c(void) {
    ticks++;
    outb(0x20, 0x20); // EOI
}

extern void pit_isr(void);  // definida no boot.asm

// ─── Delay ──────────────────────────────────────────────────────────────────

static void delay_ms(uint32_t ms) {
    uint32_t start = ticks;
    while (ticks - start < ms);
}

// ─── VGA ────────────────────────────────────────────────────────────────────

#define VGA_ADDRESS 0xB8000
#define VGA_COLS    80
#define VGA_ROWS    25

void DataLog(int row, int col, const char* text) {
    volatile char* vram = (volatile char*) VGA_ADDRESS;
    int c = col, r = row;

    while (*text) {
        if (c >= VGA_COLS) { c = 0; r++; }
        if (r >= VGA_ROWS) return;

        if (*text == '\n') { c = 0; r++; text++; continue; }

        int offset = (r * VGA_COLS + c) * 2;
        vram[offset]     = *text;
        vram[offset + 1] = 0x07;
        text++; c++;
    }
}

// ─── Entry point ─────────────────────────────────────────────────────────────
void clear_screen(void) {
    volatile char* vram = (volatile char*) VGA_ADDRESS;
    for (int i = 0; i < VGA_COLS * VGA_ROWS * 2; i += 2) {
        vram[i]     = ' ';
        vram[i + 1] = 0x07;
    }
}

void kernel_main(void) {
    clear_screen();

    // 1. Primeiro mostra algo na tela para confirmar que o boot funcionou
    DataLog(0, 0, "--------------- Octagon beta -----------------------");
    DataLog(1, 0, "Welcome to beta test of OctagonOS!\n This ISO is only for testing purposes\n restricted version with only PIT timer and VGA text mode working");
    // 2. Remapeia o PIC ANTES de qualquer outra coisa
    pic_remap();

    // 3. Configura a IDT com o handler correto
    idt_set_gate(0x20, (uint32_t)pit_isr);
    idt_install();

    // 4. Só agora inicializa o PIT (começa a gerar IRQs)
    pit_init(1000);

    // 5. Habilita interrupções por último
    __asm__ volatile ("sti");

    DataLog(2, 0, "Test Time Delay: 5 s...");

    delay_ms(5000);
    DataLog(3, 0, "5s completed");
    delay_ms(2000);

    while (1) {
        __asm__ volatile ("hlt");
    }
}