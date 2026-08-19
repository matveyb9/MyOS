#ifndef MYOS_SYSCALL_H
#define MYOS_SYSCALL_H

#include <stdint.h>

#define MYOS_SYS_WRITE UINT64_C(1)
#define MYOS_SYS_EXIT UINT64_C(2)
#define MYOS_SYS_READ UINT64_C(3)
#define MYOS_SYS_TICKS UINT64_C(4)
#define MYOS_SYS_FREE_FRAMES UINT64_C(5)
#define MYOS_SYS_SPAWN UINT64_C(6)
#define MYOS_SYS_WAIT UINT64_C(7)
#define MYOS_SYS_GETPID UINT64_C(8)
#define MYOS_SYS_SLEEP UINT64_C(9)
#define MYOS_SYS_TASK_INFO UINT64_C(10)
#define MYOS_SYS_VFS_ENTRY UINT64_C(11)
#define MYOS_SYS_VFS_READ UINT64_C(12)
#define MYOS_SYS_REBOOT UINT64_C(13)
#define MYOS_SYS_POWEROFF UINT64_C(14)
#define MYOS_SYS_UPTIME UINT64_C(15)
#define MYOS_SYS_RTC_TIME UINT64_C(16)
#define MYOS_SYS_KILL UINT64_C(17)
#define MYOS_SYS_TMPFS_CREATE UINT64_C(18)
#define MYOS_SYS_TMPFS_WRITE UINT64_C(19)
#define MYOS_SYS_TMPFS_REMOVE UINT64_C(20)
#define MYOS_SYS_PIPE_CREATE UINT64_C(21)
#define MYOS_SYS_PIPE_ATTACH_READER UINT64_C(22)
#define MYOS_SYS_PIPE_ATTACH_WRITER UINT64_C(23)
#define MYOS_SYS_PIPE_SEAL UINT64_C(24)
#define MYOS_SYS_PERSIST_CREATE UINT64_C(25)
#define MYOS_SYS_PERSIST_WRITE UINT64_C(26)
#define MYOS_SYS_PERSIST_REMOVE UINT64_C(27)
#define MYOS_SYS_GUI_SESSION UINT64_C(28)
#define MYOS_SYS_VFS_LIST UINT64_C(29)
#define MYOS_SYS_VFS_CREATE_FILE UINT64_C(30)
#define MYOS_SYS_VFS_CREATE_DIRECTORY UINT64_C(31)
#define MYOS_SYS_VFS_WRITE UINT64_C(32)
#define MYOS_SYS_VFS_REMOVE UINT64_C(33)

#define MYOS_GUI_BEGIN UINT64_C(0)
#define MYOS_GUI_INPUT UINT64_C(1)
#define MYOS_GUI_END UINT64_C(2)
#define MYOS_GUI_SET_CONTENT UINT64_C(3)

#define MYOS_GUI_CONTENT_FLAG_EDITABLE UINT64_C(1)
#define MYOS_GUI_CONTENT_FLAG_LAUNCHER UINT64_C(2)

#define MYOS_INPUT_KEY_LEFT UINT8_C(0x80)
#define MYOS_INPUT_KEY_RIGHT UINT8_C(0x81)
#define MYOS_INPUT_KEY_UP UINT8_C(0x82)
#define MYOS_INPUT_KEY_DOWN UINT8_C(0x83)
#define MYOS_INPUT_KEY_DELETE UINT8_C(0x84)
#define MYOS_INPUT_KEY_HOME UINT8_C(0x85)
#define MYOS_INPUT_KEY_END UINT8_C(0x86)
#define MYOS_INPUT_KEY_CTRL_Q UINT8_C(0x11)
#define MYOS_INPUT_KEY_ALT_TAB UINT8_C(0x1c)
#define MYOS_INPUT_KEY_ALT_F4 UINT8_C(0x1d)
#define MYOS_INPUT_GUI_ACTION_EXIT UINT8_C(0x89)
#define MYOS_INPUT_GUI_ACTION_SYSTEM UINT8_C(0x8a)
#define MYOS_INPUT_GUI_ACTION_NOTES UINT8_C(0x8b)
#define MYOS_INPUT_GUI_ACTION_EDITOR UINT8_C(0x8c)
#define MYOS_INPUT_GUI_ACTION_HOME UINT8_C(0x8d)

#define MYOS_EXIT_STATUS_KILLED UINT64_C(137)

#define MYOS_TASK_SLOT_COUNT UINT64_C(16)
#define MYOS_TASK_NAME_MAX UINT64_C(16)
#define MYOS_VFS_NAME_MAX UINT64_C(64)
#define MYOS_VFS_PATH_MAX UINT64_C(112)
#define MYOS_VFS_READ_CHUNK UINT64_C(256)
#define MYOS_VFS_OBJECT_REGULAR UINT64_C(1)
#define MYOS_VFS_OBJECT_DIRECTORY UINT64_C(2)
#define MYOS_VFS_OBJECT_SYMBOLIC_LINK UINT64_C(3)
#define MYOS_VFS_OBJECT_VIRTUAL UINT64_C(4)
#define MYOS_GUI_CONTENT_TITLE_MAX UINT64_C(16)
#define MYOS_GUI_CONTENT_MAX UINT64_C(128)
#define MYOS_TMPFS_WRITE_CHUNK UINT64_C(128)
#define MYOS_PERSIST_WRITE_CHUNK UINT64_C(128)
#define MYOS_SPAWN_PATH_MAX MYOS_VFS_PATH_MAX
#define MYOS_SPAWN_ARGUMENTS_MAX UINT64_C(128)

#define MYOS_TASK_STATE_UNUSED UINT64_C(0)
#define MYOS_TASK_STATE_READY UINT64_C(1)
#define MYOS_TASK_STATE_RUNNING UINT64_C(2)
#define MYOS_TASK_STATE_SLEEPING UINT64_C(3)
#define MYOS_TASK_STATE_ZOMBIE UINT64_C(4)
#define MYOS_TASK_STATE_WAITING UINT64_C(5)
#define MYOS_TASK_STATE_INPUT UINT64_C(6)

#define MYOS_TASK_KIND_KERNEL UINT64_C(0)
#define MYOS_TASK_KIND_USER UINT64_C(1)

struct myos_spawn_request {
    char path[MYOS_SPAWN_PATH_MAX];
    char arguments[MYOS_SPAWN_ARGUMENTS_MAX];
    uint64_t input_pipe_id;
    uint64_t output_pipe_id;
};

struct myos_task_info {
    uint64_t id;
    uint64_t state;
    uint64_t kind;
    uint64_t run_count;
    uint64_t exit_status;
    char name[MYOS_TASK_NAME_MAX];
};

struct myos_rtc_time {
    uint16_t year;
    uint8_t month;
    uint8_t day;
    uint8_t hour;
    uint8_t minute;
    uint8_t second;
};

struct myos_vfs_entry {
    uint64_t size;
    char name[MYOS_VFS_NAME_MAX];
};

struct myos_vfs_read_request {
    uint64_t offset;
    char path[MYOS_VFS_PATH_MAX];
    uint8_t data[MYOS_VFS_READ_CHUNK];
};

struct myos_vfs_directory_entry {
    char name[MYOS_VFS_NAME_MAX];
    uint64_t size;
    uint64_t type;
};

struct myos_vfs_list_request {
    uint64_t index;
    char path[MYOS_VFS_PATH_MAX];
    struct myos_vfs_directory_entry entry;
};

struct myos_vfs_path_request {
    char path[MYOS_VFS_PATH_MAX];
};

struct myos_vfs_write_request {
    uint64_t offset;
    uint64_t length;
    char path[MYOS_VFS_PATH_MAX];
    uint8_t data[MYOS_VFS_READ_CHUNK];
};

struct myos_gui_content_request {
    uint64_t length;
    uint64_t flags;
    uint64_t cursor;
    uint64_t viewport;
    char title[MYOS_GUI_CONTENT_TITLE_MAX];
    uint8_t data[MYOS_GUI_CONTENT_MAX];
};

struct myos_tmpfs_path_request {
    char path[MYOS_VFS_NAME_MAX];
};

struct myos_tmpfs_write_request {
    uint64_t offset;
    uint64_t length;
    char path[MYOS_VFS_NAME_MAX];
    uint8_t data[MYOS_TMPFS_WRITE_CHUNK];
};

void syscall_init(void);
uint64_t syscall_count(void);
uint64_t syscall_write_count(void);

#endif
