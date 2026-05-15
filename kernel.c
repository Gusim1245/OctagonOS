#include <stdint.h>

// ─── I/O ─────────────────────────────────────────────────────────────────────

static inline uint8_t inb(uint16_t port) {
    uint8_t value;
    __asm__ volatile ("inb %1, %0" : "=a"(value) : "Nd"(port));
    return value;
}

static inline void outb(uint16_t port, uint8_t value) {
    __asm__ volatile ("outb %0, %1" : : "a"(value), "Nd"(port));
}

static void io_wait(void) {
    outb(0x80, 0x00);
}

// ─── VGA ─────────────────────────────────────────────────────────────────────

#define VGA_ADDRESS 0xB8000
#define VGA_COLS    80
#define VGA_ROWS    25

void clear_screen(void) {
    volatile char* vram = (volatile char*) VGA_ADDRESS;
    for (int i = 0; i < VGA_COLS * VGA_ROWS * 2; i += 2) {
        vram[i]     = ' ';
        vram[i + 1] = 0x07;
    }
}

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


// ─── GDT ─────────────────────────────────────────────────────────────────────

struct gdt_entry {
    uint16_t limit_low;
    uint16_t base_low;
    uint8_t  base_middle;
    uint8_t  access;
    uint8_t  granularity;
    uint8_t  base_high;
} __attribute__((packed));

struct gdt_ptr {
    uint16_t limit;
    uint32_t base;
} __attribute__((packed));

static struct gdt_entry gdt[3];
static struct gdt_ptr   gdtr;

static void gdt_set_gate(int num, uint32_t base, uint32_t limit,
                         uint8_t access, uint8_t gran) {
    gdt[num].base_low    = base & 0xFFFF;
    gdt[num].base_middle = (base >> 16) & 0xFF;
    gdt[num].base_high   = (base >> 24) & 0xFF;
    gdt[num].limit_low   = limit & 0xFFFF;
    gdt[num].granularity = ((limit >> 16) & 0x0F) | (gran & 0xF0);
    gdt[num].access      = access;
}

static void gdt_install(void) {
    gdtr.limit = sizeof(gdt) - 1;
    gdtr.base  = (uint32_t)&gdt;

    gdt_set_gate(0, 0, 0,          0x00, 0x00); // null
    gdt_set_gate(1, 0, 0xFFFFFFFF, 0x9A, 0xCF); // code
    gdt_set_gate(2, 0, 0xFFFFFFFF, 0x92, 0xCF); // data

    __asm__ volatile (
        "lgdt %0\n"
        "mov $0x10, %%ax\n"
        "mov %%ax, %%ds\n"
        "mov %%ax, %%es\n"
        "mov %%ax, %%fs\n"
        "mov %%ax, %%gs\n"
        "mov %%ax, %%ss\n"
        "ljmp $0x08, $1f\n"
        "1:\n"
        : : "m"(gdtr) : "eax"
    );
}


// ─── IDT ─────────────────────────────────────────────────────────────────────

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
    idt[vector].selector  = 0x08;
    idt[vector].zero      = 0;
    idt[vector].flags     = 0x8E;
    idt[vector].base_high = (handler >> 16) & 0xFFFF;
}

static void idt_install(void) {
    idtr.limit = sizeof(idt) - 1;
    idtr.base  = (uint32_t)&idt;
    __asm__ volatile ("lidt %0" : : "m"(idtr));
}


// ─── PIC ─────────────────────────────────────────────────────────────────────

static void pic_remap(void) {
    outb(0x20, 0x11); outb(0xA0, 0x11); io_wait();
    outb(0x21, 0x20); outb(0xA1, 0x28); io_wait();
    outb(0x21, 0x04); outb(0xA1, 0x02); io_wait();
    outb(0x21, 0x01); outb(0xA1, 0x01); io_wait();
    outb(0x21, 0xFC); // IRQ0 (timer) e IRQ1 (teclado)
    outb(0xA1, 0xFF);
}


// ─── PIT ─────────────────────────────────────────────────────────────────────

#define PIT_FREQUENCY 1193182
volatile uint32_t ticks = 0;

static void pit_init(uint32_t frequency) {
    uint32_t divisor = PIT_FREQUENCY / frequency;
    outb(0x43, 0x36);
    outb(0x40, divisor & 0xFF);
    outb(0x40, (divisor >> 8) & 0xFF);
}

void pit_handler_c(void) {
    ticks++;
    outb(0x20, 0x20);
}

extern void pit_isr(void);
extern void keyboard_isr(void);


// ─── Delay ───────────────────────────────────────────────────────────────────

static void delay_ms(uint32_t ms) {
    uint32_t start = ticks;
    while (ticks - start < ms);
}

// ─── Reboot ──────────────────────────────────────────────────────────────────
void reboot(void) {
    DataLog(10, 0, "F1: Reiniciando...");

    // Reactiva interrupcoes para o delay funcionar
    pic_remap();
    idt_set_gate(0x20, (uint32_t)pit_isr);
    idt_set_gate(0x21, (uint32_t)keyboard_isr);
    idt_install();
    pit_init(1000);
    __asm__ volatile ("sti");

    delay_ms(1000);

    __asm__ volatile ("cli");

    uint8_t val;
    do {
        val = inb(0x64);
        if (val & 0x01) inb(0x60);
    } while (val & 0x02);
    outb(0x64, 0xFE);

    struct idt_ptr bad = {0, 0};
    __asm__ volatile (
        "lidt %0\n"
        "int $0x00\n"
        : : "m"(bad)
    );

    __asm__ volatile ("hlt");
}

// ─── Teclado ─────────────────────────────────────────────────────────────────

#define KEY_F1  0x3B
#define KEY_F2  0x3C
#define KEY_F3  0x3D
#define KEY_F4  0x3E
#define KEY_F5  0x3F
#define KEY_F6  0x40
#define KEY_F7  0x41
#define KEY_F8  0x42
#define KEY_F9  0x43
#define KEY_F10 0x44
#define KEY_F11 0x57
#define KEY_F12 0x58

// Delay declarado antes do on_key para poder usar aqui

static void on_key(uint8_t scancode) {
    if (scancode == KEY_F1) {
        reboot();
    } else if (scancode == KEY_F2) {
        DataLog(10, 0, "F2 pressionado!   ");
    } else if (scancode == KEY_F3) {
        DataLog(10, 0, "F3 pressionado!   ");
    } else if (scancode == KEY_F4) {
        DataLog(10, 0, "F4 pressionado!   ");
    } else if (scancode == KEY_F5) {
        DataLog(10, 0, "F5 pressionado!   ");
    } else if (scancode == KEY_F6) {
        DataLog(10, 0, "F6 pressionado!   ");
    } else if (scancode == KEY_F7) {
        DataLog(10, 0, "F7 pressionado!   ");
    } else if (scancode == KEY_F8) {
        DataLog(10, 0, "F8 pressionado!   ");
    } else if (scancode == KEY_F9) {
        DataLog(10, 0, "F9 pressionado!   ");
    } else if (scancode == KEY_F10) {
        DataLog(10, 0, "F10 pressionado!  ");
    } else if (scancode == KEY_F11) {
        DataLog(10, 0, "F11 pressionado!  ");
    } else if (scancode == KEY_F12) {
        DataLog(10, 0, "F12 pressionado!  ");
    }
}

void keyboard_handler_c(void) {
    uint8_t scancode = inb(0x60);
    if (!(scancode & 0x80)) {
        on_key(scancode);
    }
    outb(0x20, 0x20); // EOI
}

// ─── Entry point ─────────────────────────────────────────────────────────────

void kernel_main(void) {
    gdt_install();
    clear_screen();

    DataLog(0, 0, "--------------- Octagon beta -----------------------");
    DataLog(1, 0, "Welcome to beta test of OctagonOS!\n This ISO is only for testing purposes\n Commands are: F1-Reboot F2-F12-Test");

    pic_remap();
    idt_set_gate(0x20, (uint32_t)pit_isr);
    idt_set_gate(0x21, (uint32_t)keyboard_isr);
    idt_install();
    pit_init(1000);

    __asm__ volatile ("sti");

    DataLog(5, 0, "Test Time Delay: 5s...");
    delay_ms(5000);
    DataLog(6, 0, "5s OK!");

    while (1) {
        __asm__ volatile ("hlt"); // dorme ate a proxima interrupcao
    }
}
