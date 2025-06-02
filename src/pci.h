#ifndef PCI_H
#define PCI_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

struct pci_address {
    uint16_t segment;
    uint8_t bus;
    uint8_t slot;
    uint8_t function;
};

struct address_range {
    uint64_t base;
    uint64_t length;
};

enum pci_device_type {
    PCI_DEVICE_REGULAR,
    PCI_DEVICE_BRIDGE,
};

struct pci_device {
    // Type of the device
    enum pci_device_type type;

    // The root bus that this device is on
    struct pci_root_bus *root_bus;

    // Address of the device on the bus
    uint8_t slot;
    uint8_t function;
};

struct pci_bridge {
    struct pci_device dev;

    // Bus number
    uint8_t bus;

    // List of devices associated with this bridge
    size_t device_count;
    struct pci_device *devices;
};

struct pci_bar {
    // The PCI device that this bar belongs to
    struct pci_device *device;

    // The bar number in context of the device
    uint8_t bar_number;

    // Base address and size of the bar
    struct address_range range;
};

struct pci_root_bus {
    uint32_t segment;
    uint8_t bus;

    // Sorted list of address ranges this root bus decodes
    size_t range_count;
    struct address_range *ranges;

    // List of devices associated with this root bus
    size_t device_count;
    struct pci_device **devices;

    // Sorted list of allocated bars associated with this root bus
    size_t bar_count;
    struct pci_bar **bars;
};

#define PCI_OFFSET_MASK (~UINT32_C(3))

#define pci_read8(address, offset) ({ \
    /* Figure out which byte we want to read by AND-ing the offset with 0x3 (0b11) */ \
    /* This gives us a value between 0 and 3 (inclusive) */ \
    uint32_t byte_index = (offset) & 0x3; \
    uint32_t dword = pci_read_config_space((address), (offset) & PCI_OFFSET_MASK); \
    /* Shift the read dword right by `byte_index * 8` bits, that moves the byte */ \
    /* we are interested in to the least significant position */ \
    (uint8_t)((dword >> (byte_index * 8)) & 0xFF); \
})

#define pci_read16(address, offset) ({ \
    /* Figure out which word we want to read by AND-ing the offset with 0x2 (0b10) */ \
    /* This gives us a value of 0 or 2 which we then use to select the right word */ \
    uint32_t word_index = (offset) & 0x2; \
    uint32_t dword = pci_read_config_space((address), (offset) & PCI_OFFSET_MASK); \
    /* Shift the read dword right by `word_index * 8` bits, that moves the word */ \
    /* we are interested in to the least significant position */ \
    (uint16_t)((dword >> (word_index * 8)) & 0xFFFF); \
})

// No bit magic required here as that case matches the granularity of pci_read_config_space
#define pci_read32(address, offset) pci_read_config_space((address), (offset))

#define pci_write8(address, offset, value) ({ \
    uint32_t byte_index = (offset) & 0x3; \
    uint32_t dword = pci_read_config_space((address), (offset) & PCI_OFFSET_MASK); \
    /* First, we create a mask which will mask out the bits we are modifying */ \
    /* by shifting 0xFF left by `byte_index * 8` and the inverting it, then */ \
    /* we mask the read dword with that mask and finally we OR in the new values */ \
    /* shifted by `byte_index * 8` which places it at the right spot in the dword */ \
    uint32_t new_dword = (dword & ~(0xFF << (byte_index * 8))) | (((value) & 0xFF) << (byte_index * 8)); \
    pci_write_config_space((address), (offset) & PCI_OFFSET_MASK, new_dword); \
})

#define pci_write16(address, offset, value) ({ \
    uint32_t byte_index = (offset) & 0x2; \
    uint32_t dword = pci_read_config_space((address), (offset) & PCI_OFFSET_MASK); \
    /* This works similarly, except we use 0xFFFF because we work with a word */ \
    /* here instead of a single byte, and the `word_index` can either be 0 or 2 */ \
    uint32_t new_dword = (dword & ~(0xFFFF << (byte_index * 8))) | (((value) & 0xFFFF) << (byte_index * 8)); \
    pci_write_config_space((address), (offset) & PCI_OFFSET_MASK, new_dword); \
})

// And here, again, no bit fiddling required as this matches the granularity of pci_write_config_space
#define pci_write32(address, offset, value) pci_write_config_space((address), (offset), (value))

// Read PCI config space of the given device at given offset.
// Offset will be aligned down to the nearest multiple of 4.
uint32_t pci_read_config_space(struct pci_address *address, uint32_t offset);

// Write PCI config space of the given device at given offset.
// Offset will be aligned down to the nearest multiple of 4.
void pci_write_config_space(struct pci_address *address, uint32_t offset, uint32_t value);

// Discover PCI root buses and devices behind them.
bool pci_initialize(void);

// Free a PCI root bus and all data associated with it.
void pci_free_root_bus(struct pci_root_bus *bus);

// Insert bar bottom up in some of the ranges of the associated root bus
// structure.
bool pci_insert_bar(struct pci_bar *bar);

// Remove the bar from the associated root bus.
// The structure is modified to reflect that by setting the base address to 0.
void pci_remove_bar(struct pci_bar *bar);

#endif
