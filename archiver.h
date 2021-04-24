#define META "ARCHIVER"
#define META_LENGTH 9

#define MAX_PATH_LENGTH 1024

enum _path_type {
    FILE_NAME = 0,
    FOLDER_NAME = 1,
};
typedef enum _path_type path_type;

void pack(char* dir_path, char* archive_path);
void _pack(int archive, char* src_path);
void _pack_info(int fd, path_type p_type, char* path);
void _pack_content(int fd, char* path);

void unpack(char* archive_path, char* out_path);
char* _rename_root(char* path, int old_root_len, char* new_root);

void _remove_extra_slash(char* path);