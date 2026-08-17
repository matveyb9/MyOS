#include <stddef.h>
#include <stdint.h>

#include <arch.h>
#include <paging.h>
#include <pmm.h>
#include <user.h>

#define USER_DEMO_CODE_ADDRESS UINT64_C(0x0000000000400000)
#define USER_DEMO_STACK_PAGE UINT64_C(0x00007FFFFFFFF000)
#define USER_DEMO_STACK_GUARD UINT64_C(0x00007FFFFFFFE000)
#define USER_DEMO_STACK_TOP UINT64_C(0x00007FFFFFFFFFF0)
#define USER_DEMO_MESSAGE_OFFSET 64U

static const uint8_t user_demo_image[] = {
    0xB8U, 0x01U, 0x00U, 0x00U, 0x00U,
    0xBFU, 0x01U, 0x00U, 0x00U, 0x00U,
    0x48U, 0x8DU, 0x35U, 0x2FU, 0x00U, 0x00U, 0x00U,
    0xBAU, 0x13U, 0x00U, 0x00U, 0x00U,
    0x0FU, 0x05U,
    0xEBU, 0xFEU,
    [USER_DEMO_MESSAGE_OFFSET] = 'M',
    [USER_DEMO_MESSAGE_OFFSET + 1U] = 'y',
    [USER_DEMO_MESSAGE_OFFSET + 2U] = 'O',
    [USER_DEMO_MESSAGE_OFFSET + 3U] = 'S',
    [USER_DEMO_MESSAGE_OFFSET + 4U] = ' ',
    [USER_DEMO_MESSAGE_OFFSET + 5U] = 'r',
    [USER_DEMO_MESSAGE_OFFSET + 6U] = 'i',
    [USER_DEMO_MESSAGE_OFFSET + 7U] = 'n',
    [USER_DEMO_MESSAGE_OFFSET + 8U] = 'g',
    [USER_DEMO_MESSAGE_OFFSET + 9U] = '3',
    [USER_DEMO_MESSAGE_OFFSET + 10U] = ' ',
    [USER_DEMO_MESSAGE_OFFSET + 11U] = 's',
    [USER_DEMO_MESSAGE_OFFSET + 12U] = 'y',
    [USER_DEMO_MESSAGE_OFFSET + 13U] = 's',
    [USER_DEMO_MESSAGE_OFFSET + 14U] = 'c',
    [USER_DEMO_MESSAGE_OFFSET + 15U] = 'a',
    [USER_DEMO_MESSAGE_OFFSET + 16U] = 'l',
    [USER_DEMO_MESSAGE_OFFSET + 17U] = 'l',
    [USER_DEMO_MESSAGE_OFFSET + 18U] = '\n'
};

static uint64_t code_frame;
static uint64_t stack_frame;
static int demo_ready;

static void release_demo_pages(void) {
    if (code_frame != 0U) {
        (void)paging_unmap_page(USER_DEMO_CODE_ADDRESS);
        (void)pmm_free_frame(code_frame);
        code_frame = 0U;
    }
    if (stack_frame != 0U) {
        (void)paging_unmap_page(USER_DEMO_STACK_PAGE);
        (void)pmm_free_frame(stack_frame);
        stack_frame = 0U;
    }
    (void)paging_unmap_page(USER_DEMO_STACK_GUARD);
}

int user_demo_prepare(void) {
    uint8_t *code;

    if (demo_ready != 0) {
        return 1;
    }
    code_frame = pmm_allocate_user_frame();
    stack_frame = pmm_allocate_user_frame();
    if (code_frame == PMM_INVALID_ADDRESS || stack_frame == PMM_INVALID_ADDRESS
        || paging_map_guard(USER_DEMO_STACK_GUARD) == 0
        || paging_map_page(USER_DEMO_CODE_ADDRESS, code_frame, PAGING_FLAG_USER | PAGING_FLAG_WRITABLE) == 0
        || paging_map_page(USER_DEMO_STACK_PAGE, stack_frame, PAGING_FLAG_USER | PAGING_FLAG_WRITABLE) == 0) {
        release_demo_pages();
        return 0;
    }

    code = (uint8_t *)(uintptr_t)USER_DEMO_CODE_ADDRESS;
    for (size_t index = 0U; index < sizeof(user_demo_image); index++) {
        code[index] = user_demo_image[index];
    }
    if (paging_map_page(USER_DEMO_CODE_ADDRESS, code_frame, PAGING_FLAG_USER) == 0) {
        release_demo_pages();
        return 0;
    }
    demo_ready = 1;
    return 1;
}

void user_demo_enter(void) {
    if (demo_ready == 0 || pmm_frame_owner(code_frame) != FRAME_OWNER_USER
        || pmm_frame_owner(stack_frame) != FRAME_OWNER_USER) {
        arch_halt();
    }
    arch_enter_user_mode(USER_DEMO_CODE_ADDRESS, USER_DEMO_STACK_TOP);
}

uint64_t user_demo_code_address(void) {
    return USER_DEMO_CODE_ADDRESS;
}

uint64_t user_demo_stack_top(void) {
    return USER_DEMO_STACK_TOP;
}
